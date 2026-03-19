import re,os,subprocess,pathlib,shutil,io,logging
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
        events=[]
        for x in range(cmn_size[0]):
            for y in range(cmn_size[1]):
                nodeid=(x<<(coordinate_bits_size+3))|(y<<3)
                for port in range(2):
                    events.append(f"{device_name}/mxp_p{port}_dat_txflit_valid,bynodeid=1,nodeid={nodeid}/")
        return ",".join(events)
    
    @staticmethod
    def decode_nodeid(nodeid:int,coordinate_bits_size:int)->tuple[int,int,int]:
        x=nodeid>>(coordinate_bits_size+3)
        y=(nodeid>>3)&(1<<coordinate_bits_size - 1)
        port=(nodeid>>2)&1
        return x,y,port

    @staticmethod
    def encode_nodeid(x:int,y:int,port:int,coordinate_bits_size:int)->int:
        return (x<<(coordinate_bits_size+3))|(y<<3)|(port<<2)

    @staticmethod
    def __test_device_mc_pos_list(device_name:str,cmn_size:tuple[int,int],coordinate_bits_size:int)->list[int]:
        workload_path=pathlib.Path(__file__).resolve().parent.parent/"bin"/"stream"
        if shutil.which("numactl"):
            device_id=re.search(r"arm_cmn_(\d+)",device_name)
            workload_cmd=f"numactl --cpubind={device_id} --membind={device_id} {workload_path}"
        else:
            workload_cmd=workload_path

        perf_cmd=f'perf stat -e {TestArmCmnMC.get_monitor_mesh_port_perf_events(device_name,cmn_size,coordinate_bits_size)} -a -x";" -- {workload_cmd}'

        perf_result = subprocess.run(perf_cmd, stderr=subprocess.PIPE, text=True, shell=True).stderr
        perf_data = pd.read_csv(io.StringIO(perf_result),sep=';', header=None, names=['count', 'event_name'], usecols=[0, 2])
        # adopt the following
        event_counts = perf_data.iloc[:, 0].to_numpy().reshape(-1,1)
        kmeans = KMeans(n_clusters=2, random_state=0).fit(event_counts)
        labels = kmeans.labels_

        perf_data['label'] = labels
        
        label_counts = perf_data['label'].value_counts()
        least_common_label = label_counts.idxmin()

        selected_perf_data = perf_data[perf_data['label'] == least_common_label]
        
        result=[]
        if selected_perf_data.shape[0] == 2:
            for index, row in selected_perf_data.iterrows():
                event_name: str = row['event_name']
                port = int(event_name[15])
                nodeid = int(event_name.split('/')[1].split(',')[2][7:])|(port<<2)
                result.append(nodeid)
                x,y,port=TestArmCmnMC.decode_nodeid(nodeid,coordinate_bits_size)
                logging.info(f'Detect RNF on node {nodeid} (x = {x}, y = {y}, port = {port})')
        else:
            logging.error(f'Failed to analyze device {device_name}')

        return []

    @staticmethod
    def test_mc_pos()->list[tuple[str,list[int]]]:
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
    mc_pos=TestArmCmnMC.test_mc_pos()
    for device_item in mc_pos:
        print(f"{device_item[0]} {len(device_item[1])}")
        for nodeid in device_item[1]:
            print(nodeid)