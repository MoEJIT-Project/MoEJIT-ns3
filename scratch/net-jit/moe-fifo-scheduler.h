#ifndef NS3_MOE_FIFO_SCHEDULER_H
#define NS3_MOE_FIFO_SCHEDULER_H

namespace ns3 {
/**
 * @brief DistributedAgingScheduler (Algorithm: sf-vq-lb-aging)
 */
class DistributedAgingScheduler : public MoeScheduler {
public:
    using EventIdType = uint64_t;

    struct LocalEvent {
        EventIdType eventId;
        MPIRankIDType rank;
        uint32_t taskId;
        
        Time tPred;
        uint32_t b;
        uint32_t span;
        uint64_t readySlot;
        
        std::map<uint32_t, uint32_t> bytesByLeaf;
        std::map<uint32_t, uint32_t> foot;

        uint32_t age = 0;
        bool wasEligible = false;
        double prio = 0.0;
        
        Time releaseTime = Seconds(-1.0);
        bool committed = false;

        uint64_t earliestSlot = 0xFFFFFFFFFFFFFFFFULL;
    };

    struct LeafBroker {
        uint32_t id;
        std::unordered_map<uint64_t, uint32_t> srcFree;
        std::unordered_map<uint64_t, uint32_t> dstFree;
        std::map<EventIdType, LocalEvent> localEvents;
        std::unordered_map<uint32_t, std::unordered_map<uint64_t, uint32_t>> grantedQuota;
        std::unordered_map<uint32_t, std::unordered_map<uint64_t, Time>> grantedPhase;
        std::unordered_map<uint64_t, Time> nextPhase;

        uint32_t GetFree(const std::unordered_map<uint64_t, uint32_t>& dict, uint64_t s, uint32_t cap) const {
            auto it = dict.find(s);
            return it == dict.end() ? cap : it->second;
        }
        void SetFree(std::unordered_map<uint64_t, uint32_t>& dict, uint64_t s, uint32_t val) {
            dict[s] = val;
        }
    };

    uint32_t m_k;
    uint32_t m_numLeafs;
    Time m_slot;
    Time m_epoch;
    Time m_Helig;
    uint32_t m_Rslot;
    uint32_t m_Wsearch;
    Time m_localGap;
    Time m_phaseGap;
    double m_alphaAge;
    uint32_t m_capacityPerSlot = 12500;
    
    enum Mode { AGING, FIFO, SJF, SRPT };
    Mode m_mode;

    std::vector<LeafBroker> m_brokers;
    bool m_timerStarted = false;

    DistributedAgingScheduler(uint32_t k, uint32_t numLeafs, Mode mode = AGING, double scale_factor = 1.0) : m_k(k), m_numLeafs(numLeafs), m_mode(mode) {
        m_brokers.resize(m_numLeafs);
        for(uint32_t i=0; i<m_numLeafs; ++i) {
            m_brokers[i].id = i;
        }
        m_slot = MicroSeconds(10); 
        m_epoch = MilliSeconds(1);
        m_Helig = MilliSeconds(100);
        m_Rslot = static_cast<uint32_t>(100000 * scale_factor); 
        m_Wsearch = 1000; 
        m_alphaAge = 1.5;
        m_phaseGap = MicroSeconds(10);
        m_localGap = MicroSeconds(20);
        m_capacityPerSlot = static_cast<uint32_t>(125000 * scale_factor * m_k);
        
        if (m_Rslot == 0) m_Rslot = 1;
        if (m_capacityPerSlot == 0) m_capacityPerSlot = 1;
    }

    bool CheckAdmission(MPIRankIDType rank, const MoeTask& task, Ptr<UniformRandomVariable> rng) override {
        if (!m_timerStarted) {
            m_timerStarted = true;
            Simulator::Schedule(m_epoch, &DistributedAgingScheduler::OnEpochTimer, this);
        }

        EventIdType eId = ((uint64_t)rank << 32) | task.taskId;
        uint32_t srcLeaf = rank / m_k;
        LeafBroker& lb = m_brokers[srcLeaf];

        if (lb.localEvents.find(eId) == lb.localEvents.end()) {
            LocalEvent e;
            e.eventId = eId;
            e.rank = rank;
            e.taskId = task.taskId;
            e.tPred = NanoSeconds(llround(task.startTime * 1e9));

            e.readySlot = std::max((uint64_t)(Simulator::Now().GetNanoSeconds() / m_slot.GetNanoSeconds()), 
                                 (uint64_t)(e.tPred.GetNanoSeconds() / m_slot.GetNanoSeconds()));
            e.b = 0;
            for (auto kv : task.sendFlows) {
                uint32_t destLeaf = kv.first / m_k;
                e.bytesByLeaf[destLeaf] += kv.second;
                e.b += kv.second;
            }
            e.span = (e.b + m_Rslot - 1) / m_Rslot; 
            if (e.span == 0) e.span = 1;
            for (auto kv : e.bytesByLeaf) e.foot[kv.first] = kv.second / (uint32_t)e.span;
            if (m_mode == AGING) { e.prio = (double)e.b; }
            else if (m_mode == FIFO) { e.prio = (double)e.tPred.GetNanoSeconds(); }
            else if (m_mode == SJF) { e.prio = (double)e.b; }
            else if (m_mode == SRPT) { e.prio = (double)e.b; }
            lb.localEvents[eId] = e;
            FastControlPath(srcLeaf);
            return false;
        }

        LocalEvent& e = lb.localEvents[eId];
        return (e.committed && Simulator::Now() >= e.releaseTime);
    }

    Time GetBackoffTime(Ptr<UniformRandomVariable> rng) override { return m_slot; }

    void FastControlPath(uint32_t srcLeaf) {
        LeafBroker& lb = m_brokers[srcLeaf];
        Time now = Simulator::Now();
        uint64_t currentSlot = now.GetNanoSeconds() / m_slot.GetNanoSeconds();
        
        std::vector<LocalEvent*> eligible;
        for (auto& kv : lb.localEvents) {
            if (!kv.second.committed && kv.second.tPred - now <= m_Helig) {
                if (m_mode == AGING) {
                    kv.second.prio = (double)kv.second.b / (1.0 + m_alphaAge * kv.second.age);
                } else if (m_mode == FIFO) {
                    kv.second.prio = (double)kv.second.tPred.GetNanoSeconds();
                } else if (m_mode == SJF) {
                    kv.second.prio = (double)kv.second.b;
                } else if (m_mode == SRPT) {
                    // Approximate remaining volume by simulating dispatch over wait slots limits
                    double dispatched = (double)kv.second.age * (double)m_capacityPerSlot; 
                    kv.second.prio = (double)kv.second.b > dispatched ? (double)kv.second.b - dispatched : 1.0;
                }
                eligible.push_back(&kv.second);
            }
        }
        std::sort(eligible.begin(), eligible.end(), [](LocalEvent* a, LocalEvent* b) {
            if (a->prio != b->prio) return a->prio < b->prio; 
            return a->tPred < b->tPred;
        });

        std::unordered_map<uint64_t, std::vector<LocalEvent*>> buckets;
        for (LocalEvent* e : eligible) {
            uint64_t base = std::max(e->readySlot, currentSlot);
            if (e->earliestSlot != 0xFFFFFFFFFFFFFFFFULL && e->earliestSlot > base) base = e->earliestSlot;
            
            for (uint64_t s = base; s < base + m_Wsearch; ++s) {
                bool ok = true;
                uint64_t failSlot = 0;
                uint32_t srcReq = e->b / e->span;
                for (uint64_t u = s; u < s + e->span; ++u) {
                    if (lb.GetFree(lb.srcFree, u, m_capacityPerSlot) < srcReq) { ok = false; failSlot = u; break; }
                    for (auto& kv : e->foot) {
                        if (lb.GetFree(lb.grantedQuota[kv.first], u, 0) < kv.second) { ok = false; failSlot = u; break; } 
                    }
                    if (!ok) break;
                }
                if (ok) {
                    for (uint64_t u = s; u < s + e->span; ++u) {
                        lb.SetFree(lb.srcFree, u, lb.GetFree(lb.srcFree, u, m_capacityPerSlot) - srcReq);
                        for (auto& kv : e->foot) lb.SetFree(lb.grantedQuota[kv.first], u, lb.GetFree(lb.grantedQuota[kv.first], u, 0) - kv.second);
                    }
                    buckets[s].push_back(e);
                    break;
                } else {
                    s = failSlot;
                }
            }
        }

        for (auto& bkv : buckets) {
            uint64_t s = bkv.first;
            auto& bucketList = bkv.second;
            std::sort(bucketList.begin(), bucketList.end(), [](LocalEvent* a, LocalEvent* b) {
                if (a->prio != b->prio) return a->prio < b->prio; 
                return a->tPred < b->tPred;
            });
            for (size_t k = 0; k < bucketList.size(); ++k) {
                LocalEvent* e = bucketList[k];
                Time localOffset = MicroSeconds(50) + NanoSeconds(k * m_localGap.GetNanoSeconds());
                Time destPhaseTime = Seconds(0);
                for (auto& kv : e->foot) {
                    if (lb.grantedPhase[kv.first].count(s)) {
                        if (lb.grantedPhase[kv.first][s] > destPhaseTime) destPhaseTime = lb.grantedPhase[kv.first][s];
                    }
                }
                Time slotStartTime = NanoSeconds(s * m_slot.GetNanoSeconds());
                Time releaseT = e->tPred;
                if (slotStartTime + localOffset > releaseT) releaseT = slotStartTime + localOffset;
                if (slotStartTime + destPhaseTime > releaseT) releaseT = slotStartTime + destPhaseTime;
                e->releaseTime = releaseT;
                e->committed = true;
            }
        }
    }

    void OnEpochTimer() {
        Time now = Simulator::Now();
        uint64_t currentSlot = now.GetNanoSeconds() / m_slot.GetNanoSeconds();
        struct Request { EventIdType eId; uint32_t srcLeaf; uint64_t earliestSlot; uint32_t span; uint32_t foot; uint32_t b; double prio; Time tPred; };
        std::unordered_map<uint32_t, std::vector<Request>> destQueues;

        for (uint32_t i = 0; i < m_brokers.size(); ++i) {
            LeafBroker& lb = m_brokers[i];
            std::vector<LocalEvent*> eligible;
            auto provisionalSrcFree = lb.srcFree;
            for (auto& kv : lb.localEvents) {
                if (!kv.second.committed && kv.second.tPred - now <= m_Helig) {
                    if (kv.second.wasEligible) { kv.second.age++; }
                    else { kv.second.age = 0; kv.second.wasEligible = true; }
                    
                    if (m_mode == AGING) {
                        kv.second.prio = (double)kv.second.b / (1.0 + 2.0 * kv.second.age);
                    } else if (m_mode == FIFO) {
                        kv.second.prio = (double)kv.second.tPred.GetNanoSeconds();
                    } else if (m_mode == SJF) {
                        kv.second.prio = (double)kv.second.b;
                    } else if (m_mode == SRPT) {
                        double dispatched = (double)kv.second.age * (double)m_capacityPerSlot; 
                        kv.second.prio = (double)kv.second.b > dispatched ? (double)kv.second.b - dispatched : 1.0;
                    }
                    eligible.push_back(&kv.second);
                }
            }
            std::sort(eligible.begin(), eligible.end(), [](LocalEvent* a, LocalEvent* b) {
                if (a->prio != b->prio) { return a->prio < b->prio; }
                return a->tPred < b->tPred;
            });
            
            // 调试日志：显示当前 Leaf 的任务优先级排序 (每 1000 个 Slot 打印一次，约 10ms)
            if (!eligible.empty() && currentSlot % 1000 == 0) {
                std::cout << "[Scheduler] Slot=" << currentSlot << " Leaf=" << i << " EligibleTasks=" << eligible.size() << " TopPrio=" << eligible[0]->prio << std::endl;
            }

            for (LocalEvent* e : eligible) {
                e->earliestSlot = 0xFFFFFFFFFFFFFFFFULL;
                uint64_t base = std::max(e->readySlot, currentSlot);
                for (uint64_t s = base; s < base + m_Wsearch; ++s) {
                    uint32_t srcReq = e->b / e->span;
                    bool ok = true;
                    uint64_t failSlot = 0;
                    for (uint64_t u = s; u < s + e->span; ++u) { 
                        if (lb.GetFree(provisionalSrcFree, u, m_capacityPerSlot) < srcReq) { ok = false; failSlot = u; break; } 
                    }
                    if (ok) {
                        e->earliestSlot = s;
                        for (uint64_t u = s; u < s + e->span; ++u) lb.SetFree(provisionalSrcFree, u, lb.GetFree(provisionalSrcFree, u, m_capacityPerSlot) - srcReq);
                        break;
                    } else {
                        s = failSlot;
                    }
                }
                if (e->earliestSlot != 0xFFFFFFFFFFFFFFFFULL) {
                    for (auto& kv : e->foot) {
                        destQueues[kv.first].push_back({e->eventId, i, e->earliestSlot, e->span, kv.second, e->b, e->prio, e->tPred});
                    }
                }
            }
        }
        
        for (uint32_t d = 0; d < m_brokers.size(); ++d) {
            LeafBroker& lb = m_brokers[d];
            std::unordered_map<uint32_t, std::vector<Request>> srcQueues;
            for (auto& req : destQueues[d]) srcQueues[req.srcLeaf].push_back(req);
            std::unordered_map<uint32_t, std::unordered_map<uint64_t, uint32_t>> newQuota;
            std::unordered_map<uint32_t, std::unordered_map<uint64_t, Time>> newPhase;
            lb.nextPhase.clear(); 
            auto provisionalDstFree = lb.dstFree;
            while (!srcQueues.empty()) {
                uint32_t bestSrc = 0xFFFFFFFF; Request* bestReq = nullptr;
                for (auto& kv : srcQueues) {
                    if (kv.second.empty()) continue;
                    Request* head = &kv.second.front();
                    if (bestReq == nullptr || head->prio < bestReq->prio || (head->prio == bestReq->prio && head->tPred < bestReq->tPred) || (head->prio == bestReq->prio && head->tPred == bestReq->tPred && head->srcLeaf < bestReq->srcLeaf)) { 
                        bestReq = head; 
                        bestSrc = kv.first; 
                    }
                }
                if (bestReq == nullptr) break;
                uint64_t start = 0xFFFFFFFFFFFFFFFFULL;
                for (uint64_t s = bestReq->earliestSlot; s < bestReq->earliestSlot + m_Wsearch; ++s) {
                    bool ok = true;
                    uint64_t failSlot = 0;
                    for (uint64_t u = s; u < s + bestReq->span; ++u) { 
                        if (lb.GetFree(provisionalDstFree, u, m_capacityPerSlot) < bestReq->foot) { ok = false; failSlot = u; break; } 
                    }
                    if (ok) { start = s; break; }
                    else { s = failSlot; }
                }
                if (start != 0xFFFFFFFFFFFFFFFFULL) {
                     // 调试日志：带宽授予决策 (每 500 个 Slot 抽样一次，约 5ms)
                    if (bestReq->foot > 0 && start % 500 == 0) { 
                        std::cout << "[Grant] T=" << Simulator::Now().GetSeconds() << "s | L" << d << " <- L" << bestSrc 
                                  << " Ev=" << bestReq->eId << " BW=" << (bestReq->foot / 1024) << "KB"
                                  << " Slot=" << start << " Prio=" << bestReq->prio << std::endl;
                    }
                    for (uint64_t u = start; u < start + bestReq->span; ++u) {
                        newQuota[bestSrc][u] += bestReq->foot;
                        lb.SetFree(provisionalDstFree, u, lb.GetFree(provisionalDstFree, u, m_capacityPerSlot) - bestReq->foot);
                        lb.SetFree(lb.dstFree, u, lb.GetFree(lb.dstFree, u, m_capacityPerSlot) - bestReq->foot);
                    }
                    if (newPhase[bestSrc].find(start) == newPhase[bestSrc].end()) {
                        if (lb.nextPhase.find(start) == lb.nextPhase.end()) lb.nextPhase[start] = Seconds(0);
                        newPhase[bestSrc][start] = lb.nextPhase[start];
                        lb.nextPhase[start] = lb.nextPhase[start] + m_phaseGap;
                    }
                }
                srcQueues[bestSrc].erase(srcQueues[bestSrc].begin());
                if (srcQueues[bestSrc].empty()) srcQueues.erase(bestSrc);
            }
            for (uint32_t sIdx = 0; sIdx < m_brokers.size(); ++sIdx) {
                for (auto& kv : newQuota[sIdx]) m_brokers[sIdx].grantedQuota[d][kv.first] += kv.second;
                for (auto& kv : newPhase[sIdx]) m_brokers[sIdx].grantedPhase[d][kv.first] = kv.second;
            }
        }
        for (uint32_t i = 0; i < m_brokers.size(); ++i) FastControlPath(i);
        Simulator::Schedule(m_epoch, &DistributedAgingScheduler::OnEpochTimer, this);
    }
};

} // namespace ns3
#endif
