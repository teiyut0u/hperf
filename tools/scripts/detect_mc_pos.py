"""
test_mc_pos.py - ARM CMN (Coherent Mesh Network) Memory Controller Position Detection Tool

This script detects the positions of Memory Controllers (MC) in ARM CMN topology
by analyzing performance counter events using perf and KMeans clustering.

Usage:
    python test_mc_pos.py <mc_pos_file>

Arguments:
    mc_pos_file: Output file path to save the detected MC positions

Example:
    python test_mc_pos.py /tmp/mc_positions.txt

Requirements:
    - Python 3.8+
    - pandas
    - scikit-learn
    - perf (Linux performance monitoring tool)
    - numactl (optional, for NUMA-aware workload binding)
    - ARM CMN PMU driver loaded (arm_cmn_* devices in /sys/bus/event_source/devices/)

Install dependencies:
    pip install pandas scikit-learn
"""

import re,os,subprocess,pathlib,shutil,io,sys
import pandas as pd
from sklearn.cluster import KMeans

class TestArmCmnMC:
    @staticmethod
    def __get_device_path_list()->list[str]:
        """
        get arm cmn devices list

        scan directory '/sys/bus/event_source/devices/' and get all matched '/sys/bus/event_source/devices/arm_cmn_%d'

        Returns:
            list[str]: A list of arm cmn devices path
        """
        regex = re.compile(r"arm_cmn_\d+")
        matches = []
    
        devices_dir="/sys/bus/event_source/devices/"
        for name in os.listdir(devices_dir):
            if regex.match(name):  # 完全匹配
                matches.append(f"{devices_dir}{name}")
        return matches

    @staticmethod
    def __get_device_name_list()->list[str]:
        """
        get arm cmn devices list

        scan directory '/sys/bus/event_source/devices/' and get all matched '/sys/bus/event_source/devices/arm_cmn_%d'

        Returns:
            list[str]: A list of arm cmn devices name
        """
        regex = re.compile(r"arm_cmn_\d+")
        matches = []
    
        devices_dir="/sys/bus/event_source/devices/"
        for name in os.listdir(devices_dir):
            if regex.match(name):  # 完全匹配
                matches.append(name)
        return matches
    
    @staticmethod
    def __get_device_name(device_path:str)->str:
        """
        get name of devices

        Args:
            device_path (str): path like '/sys/bus/event_source/devices/arm_cmn_\\d+'
        
        Returns:
            str: name like 'arm_cmn_\\d+'
        """
        return device_path[30:]

    @staticmethod
    def __test_xp_exists(device_name:str,nodeid:int)->bool:
        """
        test whether specified device support xp specified by nodeid

        Args:
            device_name (str): name like 'arm_cmn_\\d+'
            nodeid (int): nodeid to specify xp. the port bit must be 0.
        
        Returns:
            bool: True if the xp is supported, else False 
        """
        perf_command = f'perf stat -e {device_name}/mxp_n_dat_txflit_valid,bynodeid=1,nodeid={nodeid}/ -a -x";" -- true'
        perf_result = subprocess.run(perf_command, stderr=subprocess.PIPE, text=True, shell=True).stderr
        if 'not supported' in perf_result.split(';')[0]:
            return False
        else:
            return True

    @staticmethod
    def __test_nodeid_bits_size_helper(device_name:str,nodeid_first:int,nodeid_last:int,nodeid_step:int,target_sign:bool):
        """
        test nodeid in [`nodeid_first`, `nodeid_last`] with step `nodeid_step`.

        if any test result equals to `target_sign`, return `True`, else `False`

        Args:
            device_name (str): name like 'arm_cmn_\\d+'
            nodeid_first (int): first nodeid to test.
            nodeid_last (int): last nodeid to test.
            nodeid_step (int): test step.
            target_sign (bool): target sign
        
        Returns:
            bool: `True` if any test result equals to `target_sign`, else `False`
        """
        for nodeid in range(nodeid_first,nodeid_last+nodeid_step,nodeid_step):
            if TestArmCmnMC.__test_xp_exists(device_name,nodeid)==target_sign:
                return True
        return False

    @staticmethod
    def test_nodeid_bits_size(device_name:str)->int:
        """
        test how many bits used to encode nodeid

        Args:
            device_name (str): name like 'arm_cmn_\\d+'

        Returns:
            int: how many bits used to encode nodeid
        """
        if TestArmCmnMC.__test_nodeid_bits_size_helper(device_name,0x200,0x200,1,True):
            return 11
        if TestArmCmnMC.__test_nodeid_bits_size_helper(device_name,0x80,0x80,1,True):
            return 9
        if TestArmCmnMC.__test_nodeid_bits_size_helper(device_name,0x0,0x20,0x8,False):
            return 7
        if TestArmCmnMC.__test_nodeid_bits_size_helper(device_name,0x28,0x38,0x8,False):
            return 9
        return 7

    @staticmethod
    def __test_coordinate_bits_size(device_name:str)->int:
        """
        test how many bits used to encode coordinate

        Args:
            device_name (str): name like 'arm_cmn_\\d+'

        Returns:
            int: how many bits used to encode coordinate
        """
        nodeid_bits_size=TestArmCmnMC.test_nodeid_bits_size(device_name)
        return (nodeid_bits_size-3)>>1

    @staticmethod
    def test_cmn_size_helper(device_name:str,coordinate_bits_size:int,coordinate_offset:int)->int:
        """
        test one coordinate's size

        Args:
            device_name (str): name like '/sys/bus/event_source/devices/arm_cmn_\\d+'
            coordinate_bits_size (int): how many bits used to encode coordinate
            coordinate_offset (int): the offset of the coordinate in bits
        
        Returns:
            int: size of the specified coordinate
        """
        left=0
        right=1<<coordinate_bits_size
        while left<right:
            mid=(left+right)>>1
            nodeid=mid<<coordinate_offset
            if TestArmCmnMC.__test_xp_exists(device_name,nodeid):
                left=mid+1
            else:
                right=mid-1
        if TestArmCmnMC.__test_xp_exists(device_name,left<<coordinate_offset):
            return left+1
        else:
            return left


    @staticmethod
    def test_cmn_size(device_name:str,coordinate_bits_size:int)->tuple[int,int]:
        """
        test cmn size

        Args:
            device_name (str): name like 'arm_cmn_\\d+'
            coordinate_bits_size (int): how many bits used to encode coordinate

        Returns:
            tuple[int,int]: size of the specified cmn
        """
        x_offset=coordinate_bits_size+3
        y_offset=3
        x=TestArmCmnMC.test_cmn_size_helper(device_name,coordinate_bits_size,x_offset)
        y=TestArmCmnMC.test_cmn_size_helper(device_name,coordinate_bits_size,y_offset)
        return (x,y)

    @staticmethod
    def get_monitor_mesh_port_perf_events(device_name:str,cmn_size:tuple[int,int],coordinate_bits_size:int):
        """
        Generate perf event string for monitoring all mesh ports.

        Creates a comma-separated list of perf events for all XP ports in the mesh.
        Events monitor `mxp_p{port}_dat_txflit_valid` for data transmission flits.

        Args:
            device_name (str): Device name like `arm_cmn_\\d+`
            cmn_size (tuple[int, int]): Mesh dimensions (x_size, y_size)
            coordinate_bits_size (int): Bits per coordinate

        Returns:
            str: Comma-separated perf event string for all mesh ports
        """
        events=[]
        for x in range(cmn_size[0]):
            for y in range(cmn_size[1]):
                nodeid=(x<<(coordinate_bits_size+3))|(y<<3)
                for port in range(2):
                    events.append(f"{device_name}/mxp_p{port}_dat_txflit_valid,bynodeid=1,nodeid={nodeid}/")
        return ",".join(events)
    
    @staticmethod
    def decode_nodeid(nodeid:int,coordinate_bits_size:int)->tuple[int,int,int]:
        """
        Decode nodeid into X, Y coordinates and port number.

        Nodeid format: [X coord | Y coord | port | 0]
        - X: highest bits
        - Y: middle bits  
        - port: bit 2 (0 or 1)
        - bit 0-1: reserved (0)

        Args:
            nodeid (int): Encoded node ID
            coordinate_bits_size (int): Bits per coordinate

        Returns:
            tuple[int, int, int]: (x, y, port) coordinates
        """
        x=nodeid>>(coordinate_bits_size+3)
        y=(nodeid>>3)&(1<<coordinate_bits_size - 1)
        port=(nodeid>>2)&1
        return x,y,port

    @staticmethod
    def encode_nodeid(x:int,y:int,port:int,coordinate_bits_size:int)->int:
        """
        Encode X, Y coordinates and port into nodeid.

        Args:
            x (int): X coordinate
            y (int): Y coordinate
            port (int): Port number (0 or 1)
            coordinate_bits_size (int): Bits per coordinate

        Returns:
            int: Encoded node ID
        """
        return (x<<(coordinate_bits_size+3))|(y<<3)|(port<<2)

    @staticmethod
    def __test_device_mc_pos_list(
        device_name:str,
        cmn_size:tuple[int,int],
        coordinate_bits_size:int,
        workload_path:pathlib.Path=pathlib.Path(__file__).resolve().parent.parent/"bin"/"test_mc_pos_workload",
    )->list[int]:
        """
        Detect Memory Controller positions for a specific device.

        Runs a memory workload and uses KMeans clustering on perf counter data
        to identify which mesh ports have high activity (indicating MC connection).

        High traffic ports are identified by clustering event counts and selecting
        the cluster with lowest frequency (typically MC ports show distinct patterns).

        Args:
            device_name (str): Device name like `arm_cmn_\\d+`
            cmn_size (tuple[int, int]): CMN mesh dimensions
            coordinate_bits_size (int): Bits per coordinate
            workload_path (pathlib.Path): Path to memory workload binary

        Returns:
            list[int]: List of nodeids corresponding to MC positions
        """
        if shutil.which("numactl"):
            device_id_match=re.search(r"arm_cmn_(\d+)",device_name)
            if device_id_match is None:
                return []
            device_id=device_id_match.group(1)
            workload_cmd=f"numactl --cpubind={device_id} --membind={device_id} {workload_path}"
        else:
            workload_cmd=workload_path

        perf_cmd=f'perf stat -e {TestArmCmnMC.get_monitor_mesh_port_perf_events(device_name,cmn_size,coordinate_bits_size)} -a -x";" -- {workload_cmd}'

        perf_result = subprocess.run(perf_cmd, stderr=subprocess.PIPE, text=True, shell=True).stderr
        with open(pathlib.Path(__file__).resolve().parent/f"test_{device_name}.log","w") as fd:
            fd.write(perf_result)
        perf_data = pd.read_csv(io.StringIO(perf_result),sep=';', header=None, names=['count', 'event_name'], usecols=[0, 2])
        # adopt the following
        event_counts = perf_data.iloc[:, 0].to_numpy().reshape(-1,1)
        kmeans = KMeans(n_clusters=4, random_state=0).fit(event_counts)
        labels = kmeans.labels_

        perf_data['label'] = labels
        
        label_counts = perf_data['label'].value_counts()
        least_common_label = label_counts.idxmin()

        selected_perf_data = perf_data[perf_data['label'] == least_common_label]
        
        result=[]
        for _, row in selected_perf_data.iterrows():
            event_name: str = row['event_name']
            port = int(event_name[15])
            nodeid = int(event_name.split('/')[1].split(',')[2][7:])|(port<<2)
            result.append(nodeid)

        return result

    @staticmethod
    def test_mc_pos()->list[tuple[str,list[int]]]:
        """
        Main entry point to detect Memory Controller positions for all CMN devices.

        Iterates through all ARM CMN devices, detects their mesh size, and
        identifies MC positions using performance counter analysis.

        Returns:
            list[tuple[str, list[int]]]: List of (device_name, mc_nodeid_list) tuples
        """
        result=[]
        device_name_list=TestArmCmnMC.__get_device_name_list()
        for device_name in device_name_list:
            coordinate_bits_size=TestArmCmnMC.__test_coordinate_bits_size(device_name)
            cmn_size=TestArmCmnMC.test_cmn_size(device_name,coordinate_bits_size)
            mc_pos_list=TestArmCmnMC.__test_device_mc_pos_list(device_name,cmn_size,coordinate_bits_size)
            result.append((device_name,mc_pos_list))
        return result

if __name__=="__main__":
    # result=TestArmCmnMC.test_cmn_size("/sys/bus/event_source/devices/arm_cmn_0",3)
    # TestArmCmnMC.test_nodeid_bits_size("/sys/bus/event_source/devices/arm_cmn_0")
    # TestArmCmnMC.test_cmn_size("/sys/bus/event_source/devices/arm_cmn_0",3)
    if len(sys.argv) != 2:
        print("Usage:\n\tpython test_mc_pos.py <mc_pos_file>",file=sys.stderr)
        sys.exit(1)
    result_path=sys.argv[1]
    mc_pos=TestArmCmnMC.test_mc_pos()
    # Write results to output file
    with open(result_path,"w") as result_output:
        for device_item in mc_pos:
            result_output.write(f"{device_item[0]} {len(device_item[1])}\n")
            for nodeid in device_item[1]:
                result_output.write(f"{nodeid}\n")