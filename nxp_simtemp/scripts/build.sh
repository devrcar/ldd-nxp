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
    make all
    if [ "$?" -ne "0" ]; then
        echo -e "${BRED}ERROR:${COLOR_OFF} Make build failed!"
        exit "${ERR_EXEC_MAKE}"
    fi
}

main; 
exit