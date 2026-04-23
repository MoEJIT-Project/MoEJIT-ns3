# NetJIT Docker Environment Guide
> For Windows 11 Users

This guide will help you set up and run the NetJIT simulation environment using Docker, without needing to manually install Linux dependencies or configure compilers.

## Prerequisites
1.  **Install Docker Desktop for Windows**:
    *   Download from [Docker Hub](https://www.docker.com/products/docker-desktop/).
    *   Ensure WSL 2 integration is enabled (usually default).

2.  **Clone this Repository**:
    ```powershell
    git clone https://github.com/FNIL-FDU-RN/ns-3.git
    cd ns-3
    git checkout moe_jit
    ```

## 1. Build the Docker Image
Open a terminal (PowerShell or Command Prompt) in the `ns-3` folder and run:

```bash
docker build -t net-jit-env .
```
*(This may take a few minutes as it downloads Ubuntu and installs build tools)*

## 2. Run the Container
Start an interactive shell inside the container:

```bash
docker run -it --name net-jit-dev -v ${PWD}:/ns-3 net-jit-env
```
*   `-it`: Interactive terminal mode.
*   `--name net-jit-dev`: Names the container for easy reference.
*   `-v ${PWD}:/ns-3`: Mounts your current directory into the container. This means any file you edit in Windows (VS Code, etc.) will instantly update inside the container, and logs generated inside will appear on your Windows machine.

> **Note for Windows PowerShell**: Use `${PWD}`. If using Command Prompt (cmd), use `%cd%` instead.

## 3. Inside the Container (Running Simulations)
Once inside the container (you should see a prompt like `root@<container_id>:/ns-3#`), you can build and run tests as if you were on Linux.

### A. Build the Project (Crucial Step!)
**You MUST rebuild the project inside the container** to ensure all library paths are correct for the Docker environment. Binaries built on your host machine (Windows/Linux) will NOT work due to absolute path mismatches.

```bash
./ns3 configure --enable-examples --enable-tests
./ns3 build
```
*(Or use CMake directly if preferred)*:
```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### B. Run the Full Regression Test
```bash
# Ensure scripts are executable
chmod +x scratch/net-jit/*.sh

# Note: The script automatically handles library paths.
# If running executables manually, you may need:
# export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/ns-3/build/lib

# Run the detailed multi-job test
./scratch/net-jit/run_multijob_detailed_full_test.sh
```

### C. Check Results
The results will be saved to `scratch/net-jit/logs/detailed_full/` inside the container, which is synced to your Windows folder. You can open `detailed_summary.csv` directly in Excel.

## 4. Stopping and Resuming
To exit the container:
```bash
exit
```

To resume work later:
```bash
docker start -ai net-jit-dev
```

## Troubleshooting
*   **"File not found" errors**: Ensure you are in the `/ns-3` directory inside the container.
*   **Permission denied**: Run `chmod +x <script_name>` on shell scripts.
*   **Slow build**: Docker on Windows can be slow with file sharing. For faster builds, avoid `-v` mounting and just `COPY` (but then you must copy logs out manually using `docker cp`). The current setup prioritizes ease of use.
