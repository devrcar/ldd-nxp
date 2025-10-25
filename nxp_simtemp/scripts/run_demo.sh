#!/bin/bash

# Terminal bold colors
BBLACK='\033[1;30m'       # Black
BRED='\033[1;31m'         # Red
BGREEN='\033[1;32m'       # Green
BYELLOW='\033[1;33m'      # Yellow
BBLUE='\033[1;34m'        # Blue
BPURPLE='\033[1;35m'      # Purple
BCYAN='\033[1;36m'        # Cyan
BWHITE='\033[1;37m'       # White

# Terminal reset color
COLOR_OFF='\033[0m'       # Text Reset


# Error Return Codes
ERR_NOT_A_FILE=2
ERR_NOT_A_DIR=20
ERR_INVALID_ARG=22
ERR_EXEC_MAKE=3

ROOT_PROJ_DIR="$(dirname $(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd))"
KERNEL_DRV_DIR="${ROOT_PROJ_DIR}/kernel"

main() {
    echo -e "${BWHITE}INFO:${COLOR_OFF} The ROOT_PROJ_DIR is at ${ROOT_PROJ_DIR}"
    if [[ ! -d "${KERNEL_DRV_DIR}" ]]; then
        echo -e "${BRED}ERROR:${COLOR_OFF} The kernel is not present at ${KERNEL_DRV_DIR}"
        exit "${ERR_NOT_A_DIR}"
    fi

    cd "${KERNEL_DRV_DIR}"
    # Instead of building, insert the driver module and the local device setup
    SIMTEMP_KO="${KERNEL_DRV_DIR}/simtemp.ko"
    LOCAL_SETUP_KO="${KERNEL_DRV_DIR}/local_device_setup.ko"

    if [[ ! -f "${SIMTEMP_KO}" ]]; then
        echo -e "${BRED}ERROR:${COLOR_OFF} Missing ${SIMTEMP_KO}"
        exit ${ERR_NOT_A_FILE}
    fi
    if [[ ! -f "${LOCAL_SETUP_KO}" ]]; then
        echo -e "${BRED}ERROR:${COLOR_OFF} Missing ${LOCAL_SETUP_KO}"
        exit ${ERR_NOT_A_FILE}
    fi

    # Helper to run commands as root (use sudo if not root)
    run_as_root() {
        if [ "$(id -u)" -eq 0 ]; then
            "$@"
        else
            sudo "$@"
        fi
    }

    echo -e "${BWHITE}INFO:${COLOR_OFF} Inserting simtemp driver (${SIMTEMP_KO})"
    run_as_root insmod "${SIMTEMP_KO}"
    if [ $? -ne 0 ]; then
        echo -e "${BRED}ERROR:${COLOR_OFF} insmod of simtemp failed"
        exit ${ERR_EXEC_MAKE}
    fi

    # Check whether the device already exists (e.g., created from device tree)
    SYSFS_DEV="/sys/class/simtemp/simtemp"
    echo -e "${BWHITE}INFO:${COLOR_OFF} Checking for device at ${SYSFS_DEV}"
    TIMEOUT=5
    while [ ${TIMEOUT} -gt 0 ]; do
        if [ -d "${SYSFS_DEV}" ]; then
            break
        fi
        sleep 1
        TIMEOUT=$((TIMEOUT - 1))
    done

    LOCAL_SETUP_LOADED=0
    if [ -d "${SYSFS_DEV}" ]; then
        echo -e "${BGREEN}INFO:${COLOR_OFF} Device found; not loading local device setup"
    else
        echo -e "${BWHITE}INFO:${COLOR_OFF} Device not found after driver insertion; loading local device setup (${LOCAL_SETUP_KO})"
        run_as_root insmod "${LOCAL_SETUP_KO}"
        if [ $? -ne 0 ]; then
            echo -e "${BRED}ERROR:${COLOR_OFF} insmod of local_device_setup failed"
            # Try to remove simtemp to leave system clean
            run_as_root rmmod simtemp 2>/dev/null || true
            exit ${ERR_EXEC_MAKE}
        fi
        LOCAL_SETUP_LOADED=1

        # Wait for the device to appear after loading the setup module
        TIMEOUT=10
        while [ ${TIMEOUT} -gt 0 ]; do
            if [ -d "${SYSFS_DEV}" ]; then
                break
            fi
            sleep 1
            TIMEOUT=$((TIMEOUT - 1))
        done

        if [ ! -d "${SYSFS_DEV}" ]; then
            echo -e "${BRED}ERROR:${COLOR_OFF} Device did not appear at ${SYSFS_DEV} after loading local_device_setup"
            # Cleanup only what we loaded
            if [ ${LOCAL_SETUP_LOADED} -eq 1 ]; then
                run_as_root rmmod local_device_setup 2>/dev/null || true
            fi
            run_as_root rmmod simtemp 2>/dev/null || true
            exit ${ERR_NOT_A_DIR}
        fi
    fi

    # Configure default values via sysfs
    echo -e "${BWHITE}INFO:${COLOR_OFF} Configuring device defaults via sysfs"
    # Defaults: sampling_ms=1000, threshold_mC=25100, mode=normal
    echo 1000 | (run_as_root tee "${SYSFS_DEV}/sampling_ms" > /dev/null)
    if [ $? -ne 0 ]; then
        echo -e "${BRED}ERROR:${COLOR_OFF} Failed to write sampling_ms"
        # continue to try other config options before exiting
    fi

    echo 25100 | (run_as_root tee "${SYSFS_DEV}/threshold_mc" > /dev/null)
    if [ $? -ne 0 ]; then
        echo -e "${BRED}ERROR:${COLOR_OFF} Failed to write threshold_mc"
    fi

    echo -n normal | (run_as_root tee "${SYSFS_DEV}/mode" > /dev/null)
    if [ $? -ne 0 ]; then
        echo -e "${BRED}ERROR:${COLOR_OFF} Failed to write mode"
    fi

    # Run the CLI test mode
    CLI="${ROOT_PROJ_DIR}/user/cli/main.py"
    if [ ! -f "${CLI}" ]; then
        echo -e "${BRED}ERROR:${COLOR_OFF} CLI not found at ${CLI}"
        # Cleanup modules (only remove local_device_setup if we loaded it)
        if [ "${LOCAL_SETUP_LOADED}" -eq 1 ]; then
            run_as_root rmmod local_device_setup 2>/dev/null || true
        fi
        run_as_root rmmod simtemp 2>/dev/null || true
        exit ${ERR_NOT_A_FILE}
    fi

    echo -e "${BWHITE}INFO:${COLOR_OFF} Running CLI test: ${CLI} --test"
    # Run with Python3 and make sure the user/ directory is on PYTHONPATH so
    # the `backend` package can be imported. Run as root if required so the
    # CLI can open the character device under /dev.
    run_as_root env PYTHONPATH="${ROOT_PROJ_DIR}/user" python3 "${CLI}" --test
    CLI_RET=$?

    if [ ${CLI_RET} -ne 0 ]; then
        echo -e "${BRED}ERROR:${COLOR_OFF} CLI returned non-zero (${CLI_RET})"
    else
        echo -e "${BGREEN}INFO:${COLOR_OFF} CLI test completed successfully"
    fi

    # Cleanup: unregister device and remove modules
    echo -e "${BWHITE}INFO:${COLOR_OFF} Cleaning up (unregister device modules)"
    if [ "${LOCAL_SETUP_LOADED}" -eq 1 ]; then
        run_as_root rmmod local_device_setup 2>/dev/null || true
    fi
    run_as_root rmmod simtemp 2>/dev/null || true

    # Exit with the CLI return code so caller can detect test result
    exit ${CLI_RET}
}

main; 
exit