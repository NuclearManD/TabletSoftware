
import os, platform, subprocess, re


def get_processor_name():
    if platform.system() == "Windows":
        return platform.processor()
    elif platform.system() == "Darwin":
        os.environ['PATH'] = os.environ['PATH'] + os.pathsep + '/usr/sbin'
        command ="sysctl -n machdep.cpu.brand_string"
        return subprocess.check_output(command).decode().strip()
    elif platform.system() == "Linux":
        command = "cat /proc/cpuinfo"
        all_info = subprocess.check_output(command, shell=True).strip()
        for line in all_info.decode().split("\n"):
            if "model name" in line:
                return re.sub( ".*model name.*:", "", line,1)
    return ""


def is_real_tablet():
    '''This function checks if we're running on a real tablet'''
    if platform.system() == "Linux":
        command = "cat /proc/cpuinfo"
        all_info = subprocess.check_output(command, shell=True).strip()
        for line in all_info.decode().split("\n"):
            if "Model" in line:
                return "Raspberry Pi" in line

    return False
