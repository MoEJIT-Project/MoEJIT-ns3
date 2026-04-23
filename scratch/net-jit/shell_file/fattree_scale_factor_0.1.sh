mkdir fattree_scale_factor_0.1_k_8_model_resnet152_measure_net-jit_bd_opt_400Gbps_opt_delay_100000000
cd fattree_scale_factor_0.1_k_8_model_resnet152_measure_net-jit_bd_opt_400Gbps_opt_delay_100000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/resnet152 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/resnet152/1.txt --measure_method=net-jit --bandwidth_opt=400Gbps --optimization_delay=100000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_resnet152_measure_net-jit_bd_opt_400Gbps_opt_delay_1000000000
cd fattree_scale_factor_0.1_k_8_model_resnet152_measure_net-jit_bd_opt_400Gbps_opt_delay_1000000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/resnet152 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/resnet152/1.txt --measure_method=net-jit --bandwidth_opt=400Gbps --optimization_delay=1000000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_resnet152_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_100000000
cd fattree_scale_factor_0.1_k_8_model_resnet152_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_100000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/resnet152 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/resnet152/1.txt --measure_method=static_window --bandwidth_opt=400Gbps --optimization_delay=100000000 --static_window_advance_time=100000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_resnet152_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_500000000
cd fattree_scale_factor_0.1_k_8_model_resnet152_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_500000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/resnet152 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/resnet152/1.txt --measure_method=static_window --bandwidth_opt=400Gbps --optimization_delay=100000000 --static_window_advance_time=500000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_resnet152_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_1000000000
cd fattree_scale_factor_0.1_k_8_model_resnet152_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_1000000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/resnet152 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/resnet152/1.txt --measure_method=static_window --bandwidth_opt=400Gbps --optimization_delay=100000000 --static_window_advance_time=1000000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_resnet152_measure_static_window_bd_opt_400Gbps_opt_delay_1000000000_static_window_advance_time_1000000000
cd fattree_scale_factor_0.1_k_8_model_resnet152_measure_static_window_bd_opt_400Gbps_opt_delay_1000000000_static_window_advance_time_1000000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/resnet152 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/resnet152/1.txt --measure_method=static_window --bandwidth_opt=400Gbps --optimization_delay=1000000000 --static_window_advance_time=1000000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_resnet152_measure_slide_window_bd_opt_400Gbps_opt_delay_100000000
cd fattree_scale_factor_0.1_k_8_model_resnet152_measure_slide_window_bd_opt_400Gbps_opt_delay_100000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/resnet152 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/resnet152/1.txt --measure_method=slide_window --bandwidth_opt=400Gbps --optimization_delay=100000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_resnet152_measure_slide_window_bd_opt_400Gbps_opt_delay_1000000000
cd fattree_scale_factor_0.1_k_8_model_resnet152_measure_slide_window_bd_opt_400Gbps_opt_delay_1000000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/resnet152 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/resnet152/1.txt --measure_method=slide_window --bandwidth_opt=400Gbps --optimization_delay=1000000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_resnet152_measure_none_bd_opt_400Gbps_opt_delay_100000000
cd fattree_scale_factor_0.1_k_8_model_resnet152_measure_none_bd_opt_400Gbps_opt_delay_100000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/resnet152 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/resnet152/1.txt --measure_method=none --bandwidth_opt=400Gbps --optimization_delay=100000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt1_measure_net-jit_bd_opt_400Gbps_opt_delay_100000000
cd fattree_scale_factor_0.1_k_8_model_gpt1_measure_net-jit_bd_opt_400Gbps_opt_delay_100000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt1 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt1/1.txt --measure_method=net-jit --bandwidth_opt=400Gbps --optimization_delay=100000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt1_measure_net-jit_bd_opt_400Gbps_opt_delay_1000000000
cd fattree_scale_factor_0.1_k_8_model_gpt1_measure_net-jit_bd_opt_400Gbps_opt_delay_1000000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt1 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt1/1.txt --measure_method=net-jit --bandwidth_opt=400Gbps --optimization_delay=1000000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt1_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_100000000
cd fattree_scale_factor_0.1_k_8_model_gpt1_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_100000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt1 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt1/1.txt --measure_method=static_window --bandwidth_opt=400Gbps --optimization_delay=100000000 --static_window_advance_time=100000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt1_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_500000000
cd fattree_scale_factor_0.1_k_8_model_gpt1_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_500000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt1 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt1/1.txt --measure_method=static_window --bandwidth_opt=400Gbps --optimization_delay=100000000 --static_window_advance_time=500000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt1_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_1000000000
cd fattree_scale_factor_0.1_k_8_model_gpt1_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_1000000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt1 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt1/1.txt --measure_method=static_window --bandwidth_opt=400Gbps --optimization_delay=100000000 --static_window_advance_time=1000000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt1_measure_static_window_bd_opt_400Gbps_opt_delay_1000000000_static_window_advance_time_1000000000
cd fattree_scale_factor_0.1_k_8_model_gpt1_measure_static_window_bd_opt_400Gbps_opt_delay_1000000000_static_window_advance_time_1000000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt1 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt1/1.txt --measure_method=static_window --bandwidth_opt=400Gbps --optimization_delay=1000000000 --static_window_advance_time=1000000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt1_measure_slide_window_bd_opt_400Gbps_opt_delay_100000000
cd fattree_scale_factor_0.1_k_8_model_gpt1_measure_slide_window_bd_opt_400Gbps_opt_delay_100000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt1 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt1/1.txt --measure_method=slide_window --bandwidth_opt=400Gbps --optimization_delay=100000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt1_measure_slide_window_bd_opt_400Gbps_opt_delay_1000000000
cd fattree_scale_factor_0.1_k_8_model_gpt1_measure_slide_window_bd_opt_400Gbps_opt_delay_1000000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt1 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt1/1.txt --measure_method=slide_window --bandwidth_opt=400Gbps --optimization_delay=1000000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt1_measure_none_bd_opt_400Gbps_opt_delay_100000000
cd fattree_scale_factor_0.1_k_8_model_gpt1_measure_none_bd_opt_400Gbps_opt_delay_100000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt1 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt1/1.txt --measure_method=none --bandwidth_opt=400Gbps --optimization_delay=100000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt2_measure_net-jit_bd_opt_400Gbps_opt_delay_100000000
cd fattree_scale_factor_0.1_k_8_model_gpt2_measure_net-jit_bd_opt_400Gbps_opt_delay_100000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt2 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt2/1.txt --measure_method=net-jit --bandwidth_opt=400Gbps --optimization_delay=100000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt2_measure_net-jit_bd_opt_400Gbps_opt_delay_1000000000
cd fattree_scale_factor_0.1_k_8_model_gpt2_measure_net-jit_bd_opt_400Gbps_opt_delay_1000000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt2 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt2/1.txt --measure_method=net-jit --bandwidth_opt=400Gbps --optimization_delay=1000000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt2_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_100000000
cd fattree_scale_factor_0.1_k_8_model_gpt2_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_100000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt2 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt2/1.txt --measure_method=static_window --bandwidth_opt=400Gbps --optimization_delay=100000000 --static_window_advance_time=100000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt2_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_500000000
cd fattree_scale_factor_0.1_k_8_model_gpt2_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_500000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt2 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt2/1.txt --measure_method=static_window --bandwidth_opt=400Gbps --optimization_delay=100000000 --static_window_advance_time=500000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt2_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_1000000000
cd fattree_scale_factor_0.1_k_8_model_gpt2_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_1000000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt2 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt2/1.txt --measure_method=static_window --bandwidth_opt=400Gbps --optimization_delay=100000000 --static_window_advance_time=1000000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt2_measure_static_window_bd_opt_400Gbps_opt_delay_1000000000_static_window_advance_time_1000000000
cd fattree_scale_factor_0.1_k_8_model_gpt2_measure_static_window_bd_opt_400Gbps_opt_delay_1000000000_static_window_advance_time_1000000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt2 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt2/1.txt --measure_method=static_window --bandwidth_opt=400Gbps --optimization_delay=1000000000 --static_window_advance_time=1000000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt2_measure_slide_window_bd_opt_400Gbps_opt_delay_100000000
cd fattree_scale_factor_0.1_k_8_model_gpt2_measure_slide_window_bd_opt_400Gbps_opt_delay_100000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt2 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt2/1.txt --measure_method=slide_window --bandwidth_opt=400Gbps --optimization_delay=100000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt2_measure_slide_window_bd_opt_400Gbps_opt_delay_1000000000
cd fattree_scale_factor_0.1_k_8_model_gpt2_measure_slide_window_bd_opt_400Gbps_opt_delay_1000000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt2 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt2/1.txt --measure_method=slide_window --bandwidth_opt=400Gbps --optimization_delay=1000000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_gpt2_measure_none_bd_opt_400Gbps_opt_delay_100000000
cd fattree_scale_factor_0.1_k_8_model_gpt2_measure_none_bd_opt_400Gbps_opt_delay_100000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/gpt2 --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/gpt2/1.txt --measure_method=none --bandwidth_opt=400Gbps --optimization_delay=100000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_bert_measure_net-jit_bd_opt_400Gbps_opt_delay_100000000
cd fattree_scale_factor_0.1_k_8_model_bert_measure_net-jit_bd_opt_400Gbps_opt_delay_100000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/bert --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/bert/1.txt --measure_method=net-jit --bandwidth_opt=400Gbps --optimization_delay=100000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_bert_measure_net-jit_bd_opt_400Gbps_opt_delay_1000000000
cd fattree_scale_factor_0.1_k_8_model_bert_measure_net-jit_bd_opt_400Gbps_opt_delay_1000000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/bert --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/bert/1.txt --measure_method=net-jit --bandwidth_opt=400Gbps --optimization_delay=1000000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_bert_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_100000000
cd fattree_scale_factor_0.1_k_8_model_bert_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_100000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/bert --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/bert/1.txt --measure_method=static_window --bandwidth_opt=400Gbps --optimization_delay=100000000 --static_window_advance_time=100000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_bert_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_500000000
cd fattree_scale_factor_0.1_k_8_model_bert_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_500000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/bert --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/bert/1.txt --measure_method=static_window --bandwidth_opt=400Gbps --optimization_delay=100000000 --static_window_advance_time=500000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_bert_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_1000000000
cd fattree_scale_factor_0.1_k_8_model_bert_measure_static_window_bd_opt_400Gbps_opt_delay_100000000_static_window_advance_time_1000000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/bert --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/bert/1.txt --measure_method=static_window --bandwidth_opt=400Gbps --optimization_delay=100000000 --static_window_advance_time=1000000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_bert_measure_static_window_bd_opt_400Gbps_opt_delay_1000000000_static_window_advance_time_1000000000
cd fattree_scale_factor_0.1_k_8_model_bert_measure_static_window_bd_opt_400Gbps_opt_delay_1000000000_static_window_advance_time_1000000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/bert --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/bert/1.txt --measure_method=static_window --bandwidth_opt=400Gbps --optimization_delay=1000000000 --static_window_advance_time=1000000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_bert_measure_slide_window_bd_opt_400Gbps_opt_delay_100000000
cd fattree_scale_factor_0.1_k_8_model_bert_measure_slide_window_bd_opt_400Gbps_opt_delay_100000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/bert --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/bert/1.txt --measure_method=slide_window --bandwidth_opt=400Gbps --optimization_delay=100000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_bert_measure_slide_window_bd_opt_400Gbps_opt_delay_1000000000
cd fattree_scale_factor_0.1_k_8_model_bert_measure_slide_window_bd_opt_400Gbps_opt_delay_1000000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/bert --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/bert/1.txt --measure_method=slide_window --bandwidth_opt=400Gbps --optimization_delay=1000000000 & 
cd .. 

mkdir fattree_scale_factor_0.1_k_8_model_bert_measure_none_bd_opt_400Gbps_opt_delay_100000000
cd fattree_scale_factor_0.1_k_8_model_bert_measure_none_bd_opt_400Gbps_opt_delay_100000000
rm -rf * 
cp ../parse-results.py . 
cp ../run_test.py . 
cp ../net-jit . 
python3 run_test.py ../../../scratch/net-jit/traces/bert --scale_factor=0.1 --k=8 --measure_traceFilePath=../../../scratch/net-jit/net-jit-traces/bert/1.txt --measure_method=none --bandwidth_opt=400Gbps --optimization_delay=100000000 & 
cd .. 

