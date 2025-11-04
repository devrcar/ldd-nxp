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
ERR_EXEC_PYTHON=4

ROOT_PROJ_DIR="$(dirname $(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd))"
KERNEL_DRV_DIR="${ROOT_PROJ_DIR}/kernel"

main() {
    echo -e "${BWHITE}INFO:${COLOR_OFF} The ROOT_PROJ_DIR is at ${ROOT_PROJ_DIR}"
    if [[ ! -d "${KERNEL_DRV_DIR}" ]]; then
        echo -e "${BRED}ERROR:${COLOR_OFF} The kernel is not present at ${KERNEL_DRV_DIR}"
        exit "${ERR_NOT_A_DIR}"
    fi

    # Building the kernel module
    echo -e "${BWHITE}INFO:${COLOR_OFF} Building the kernel module ..."
    cd "${KERNEL_DRV_DIR}"
    make all
    if [ "$?" -ne "0" ]; then
        echo -e "${BRED}ERROR:${COLOR_OFF} Make build failed!"
        exit "${ERR_EXEC_MAKE}"
    fi

    # Creating the virtual environment for the GUI
    USR_DRV_DIR="${ROOT_PROJ_DIR}/user"
    if [[ ! -d "${USR_DRV_DIR}" ]]; then
        echo -e "${BRED}ERROR:${COLOR_OFF} The user-space directory is not present at ${USR_DRV_DIR}"
        exit "${ERR_NOT_A_DIR}"
    fi

    cd "${USR_DRV_DIR}"
    if [[ ! -d "${USR_DRV_DIR}/.venv" ]]; then
        echo -e "${BWHITE}INFO:${COLOR_OFF} Creating the virtual environment for the GUI ..."
        python3 -m venv .venv
        if [ "$?" -ne "0" ]; then
            echo -e "${BRED}ERROR:${COLOR_OFF} Virtual environment creation failed!"
            exit "${ERR_EXEC_PYTHON}"
        fi
    fi
    source .venv/bin/activate

    # Installing Python dependencies
    PYTHON_DEPS=$(sed '/^#/d; /^[[:space:]]*$/d' "${USR_DRV_DIR}/requirements.txt")
    echo -e "${BWHITE}INFO:${COLOR_OFF} The following Python package dependencies will be installed:"
    echo "${PYTHON_DEPS}"
    echo -e "${BWHITE}INFO:${COLOR_OFF} Installing dependencies ..."

    pip install -q -r requirements.txt
    if [ "$?" -ne "0" ]; then
        echo -e "${BRED}ERROR:${COLOR_OFF} Python dependencies installation failed!"
        exit "${ERR_EXEC_PYTHON}"
    fi

    echo -e "${BGREEN}INFO:${COLOR_OFF} Dependencies installed successfully."
}

main; 
exit