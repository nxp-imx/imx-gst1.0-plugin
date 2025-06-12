#!/usr/bin/env python3
import os
import sys
import time
import fnmatch
import argparse
import datetime
import subprocess
import socket
import http.server
import signal
from enum import Enum

VERSION_STR = "PIPEWIRE_RECORDER_00.00.05"

# Return result
class Ret(Enum):
    OK = 0
    PARAM_CHANGE = 1
    ERROR = -1
    ERROR_FILE_NO_FOUND = -2

# Configure and enable pipewire backend
class PipewireBackend:
    def is_exist (self):
        pipewire_path = "/usr/lib/libweston-14/pipewire-backend.so"
        if os.path.exists (pipewire_path) and os.path.isfile(pipewire_path):
            return True
        else:
            return False

    def set_parameters(self, is_add):
        ret = Ret.OK
        file_name = "/etc/xdg/weston/weston.ini"
        section = ["[output]", "name=pipewire", "mirror-of=HDMI-A-1", "mode=1920x1080@30"]
        append_str=f"{section[0]}\n{section[1]}\n{section[2]}\n{section[3]}"
        has_pipewire = False
        remove_line = 0

        try:
            with open(file_name, "r+", encoding="utf-8") as file:
                lines = file.readlines()
                line_idx = 0
                file.seek(0)
                file.truncate()

                for line in lines:
                    use_g2d = "use-g2d=true"
                    use_g2d_pos = line.find(use_g2d)

                    if use_g2d_pos >= 0:
                        # 1.Check and disable g2d render if needed
                        if use_g2d_pos == 0:
                            line = "#" + line
                            print ("Disable g2d render")
                            ret = Ret.PARAM_CHANGE
                        elif use_g2d_pos == 1 and line.find("#") == 0:
                            # Restore the g2d parameters
                            if is_add == False:
                                line = line[1:]
                                ret = Ret.PARAM_CHANGE
                        else:
                            print ("ERROR: Undefined rules for operating g2d render, failed to disable it")
                    else:
                        # 2.Check if the pipewire paramters are added
                        if (line_idx + 2) < len(lines):
                            output_pos = line.find(section[0])
                            next_line = lines[line_idx + 1]
                            name_pos = next_line.find(section[1])                
                            # Pipewire parameters have been added already 
                            if output_pos == 0 and name_pos == 0:    
                                has_pipewire = True   
                        elif (line_idx + 1) == len(lines):
                            # Append the paramter at the end of the file
                            if has_pipewire == False:
                                has_pipewire = True
                                line = line + append_str
                                print ("Add the parameters for pipewire output")
                                ret = Ret.PARAM_CHANGE

                        # Restore the pipewire parameters
                        if is_add == False and has_pipewire:
                            if remove_line < len(section):
                                line = ""
                                remove_line += 1
                            else:
                                remove_line = 0
                            #print (f"remove line={remove_line}")
                            ret = Ret.PARAM_CHANGE 

                    line_idx += 1
                    if remove_line == 0:
                        file.write (line)
                file.flush

        except FileNotFoundError:
            print ("Error: fail to configure the file: {file_name}")
            ret = Ret.ERROR_FILE_NO_FOUND

        return ret

    def set_backend(self, is_enable):
        ret = Ret.OK
        file_name = "/lib/systemd/system/weston.service"
        keyword = "ExecStart=/usr/bin/weston"
        append_str = " --backends=drm,pipewire"

        try:
            with open(file_name, "r+", encoding="utf-8") as file:
                lines = file.readlines()
                file.seek(0)
                file.truncate()
                for line in lines:
                    keyword_pos = line.find(keyword)
                    append_str_pos = line.find(append_str)

                    if keyword_pos >= 0 and append_str_pos == -1:
                        line = line[:keyword_pos + len(keyword)] + append_str + line[keyword_pos + len(keyword):]
                        print ("Add pipewire backend")
                        ret = Ret.PARAM_CHANGE
                    elif keyword_pos >= 0 and append_str_pos >= 0:
                        if is_enable == False:
                            line = line[:append_str_pos] + line[append_str_pos + len(append_str):]
                            #print (f"Remove pipewire backend, line={line}")
                            ret = Ret.PARAM_CHANGE
                    file.write (line)
                file.flush 
        except FileNotFoundError:
            print ("Error: fail to configure the file: {file_name}")
            ret = Ret.ERROR_FILE_NO_FOUND
        return ret

    def enable (self):
        ret = Ret.ERROR
        if self.is_exist() == False:
            print (f"ERROR: failed to find pipeiwre backend file, please install it first!")
            return ret
        # Set the parameters first and then enable the backend
        res_1 = self.set_parameters(True)
        if res_1 == Ret.OK or res_1 == Ret.PARAM_CHANGE:
            res_2 = self.set_backend(True)
            if res_2 == Ret.OK or res_2 == Ret.PARAM_CHANGE:
                if res_1 == Ret.PARAM_CHANGE or res_2 == Ret.PARAM_CHANGE:
                    os.system ("systemctl daemon-reload")
                    os.system ("systemctl restart weston")
                    print ("Change weston parameters, restart weston server now to enable pipewire backend")
                    time.sleep(2)
                ret = Ret.OK
        if ret == Ret.ERROR:
            print (f"ERROR: failed to enable pipeiwre backend!")
        return ret

    def disable (self):
        ret = Ret.ERROR
        if self.is_exist() == False:
            print (f"ERROR: failed to find pipeiwre backend file, please install it first!")
            return ret
        # Disable the backend first and then remove the parameters
        res_1 = self.set_backend(False)
        if res_1 == Ret.OK or res_1 == Ret.PARAM_CHANGE:
            res_2 = self.set_parameters(False)
            if res_2 == Ret.OK or res_2 == Ret.PARAM_CHANGE:
                if res_1 == Ret.PARAM_CHANGE or res_2 == Ret.PARAM_CHANGE:
                    os.system ("systemctl daemon-reload")
                    os.system ("systemctl restart weston")
                    print ("Change weston parameters, restart weston server now to disable pipewire backend")
                ret = Ret.OK
        if ret == Ret.ERROR:
            print (f"ERROR: failed to disable pipeiwre backend!")
        return ret

# Check and start pipewire and the related servers if needed
class PipewireServer:
    def is_running(self):
        daemon_str = "/usr/bin/pipewire"
        str = os.popen(f"ps aux | grep {daemon_str}").read()
        daemon_line = str.split("\n")
        if daemon_line:
            id = daemon_line[0].split(" ")
            if (id[-1] == daemon_str):
                return True
        return False
    def start(self):
        os.system("systemctl --user --now enable pipewire wireplumber pipewire-pulse")
        #print ("Start pipewire server")

# Screen recorder base class
class ScreenRecorder:
    keepalive_time = 33
    def get_recorder_id(self):
        str = os.popen ('pw-top -b -n 1 | grep weston.pipewire | awk -F " " \'{print $2}\'').read()
        id = str.split("\n")
        return id[0]
    def get_local_ip(self):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(('8.8.8.8', 80))
            local_ip = s.getsockname()[0]
        finally:
            s.close()
        return local_ip

    def start (self, device):
        # Try to configure backend and enable it
        ret = device.enable()
        # Get recorder id only if backend is valid
        if ret == Ret.OK:
            self.recorder_id = self.get_recorder_id()
            if self.recorder_id == "":
                ret = Ret.ERROR
                print ("ERROR: failed to get pipeiwre recorder id!")
        return ret

    def stop (self):
        print ("Close screen recording")

    def clear_file (self, path, file_name):
        all_files = os.listdir(path)
        for file in all_files:
            if fnmatch.fnmatch(file, file_name):
                os.remove (file)

# Record screen data to the file
class FileRecorder (ScreenRecorder):
    process = ""
    def start (self, device):
        if super().start(device) == Ret.OK:
            # Start recording and store it to the file
            current_time = datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
            FILE_PATH = f"screen_record_{current_time}.mkv"
            record_cmd = f"gst-launch-1.0 pipewiresrc path={self.recorder_id} keepalive-time={self.keepalive_time} ! imxvideoconvert_g2d ! v4l2h264enc extra-controls=\"encode, video_bitrate=10000000\" ! h264parse ! queue ! matroskamux ! filesink location={FILE_PATH} >>/dev/null 2>&1"
            self.process = subprocess.Popen(record_cmd, shell=True)
            print ("Start recording to the file")
            return Ret.OK
        else:
            print ("ERROR: failed to start recording to the file")
            return Ret.ERROR

    def stop (self):
        if self.process != "":
            self.process.terminate()
            print ("Stop file recording to the file")

# Record screen data to HLS stream
class HlsRecorder (ScreenRecorder):
    hls_playlist = "screen_play.m3u8"
    hls_segment = "screen_segment"
    http_process = ""
    hls_process = ""
    port = 9999
    
    def start (self, device):
        if super().start(device) == Ret.OK:
            # Clear out the remaining files if they exist
            work_path = os.getcwd()
            self.clear_file (work_path, self.hls_playlist)
            self.clear_file (work_path, f"{self.hls_segment}*.ts")

            # Start http server
            http_server_cmd = f"nohup python3 -m http.server {self.port} --directory {work_path} >>/dev/null 2>&1"
            self.http_process = subprocess.Popen(http_server_cmd, shell=True)
            
            # Start recording by HLS
            server_ip = self.get_local_ip()
            record_cmd = f"gst-launch-1.0 pipewiresrc path={self.recorder_id} keepalive-time={self.keepalive_time} ! imxvideoconvert_g2d ! v4l2h264enc extra-controls=\"encode, video_bitrate=10000000\" ! h264parse ! queue ! mpegtsmux ! hlssink playlist-root=http://{server_ip}:{self.port} playlist-location={self.hls_playlist} location={self.hls_segment}_%05d.ts target-duration=1 max-files=5 >>/dev/null 2>&1"
            self.hls_process = subprocess.Popen(record_cmd, shell=True)
            print (f"Start recording as HLS server. Play URI: http://{server_ip}:{self.port}/{self.hls_playlist}")
            return Ret.OK
        else:
            print ("ERROR: failed to start recording as HLS server")
            return Ret.ERROR

    def stop (self):
        if self.hls_process != "":
            self.hls_process.terminate()
        if self.http_process != "":
            self.http_process.terminate()
        print ("Stop recording as a HLS server")

        # Clear out the remaining files
        time.sleep(1)
        work_path = os.getcwd()
        self.clear_file (work_path, self.hls_playlist)
        self.clear_file (work_path, f"{self.hls_segment}*.ts")

# Record screen data to RTSP stream
class RtspRecorder (ScreenRecorder):
    rtsp_server = "test-launch"
    rtsp_process = ""

    def rtsp_server_is_exist (self, server_name):
        all_files = os.listdir()
        for file in all_files:
            if fnmatch.fnmatch(file, server_name):
                return True
        return False
    
    def start (self, device):
        if super().start(device) == Ret.OK:
            # Install server if needed
            if self.rtsp_server_is_exist(self.rtsp_server) == False:
                os.system (f"wget http://lsv03893.swis.in-blr01.nxp.com/jaredHu/{self.rtsp_server}")
                current_path = os.getcwd()
                os.system (f"chmod +x {current_path}/{self.rtsp_server}")
            
            # Start recording 
            server_ip = self.get_local_ip()
            record_cmd = f"./{self.rtsp_server} \"pipewiresrc path={self.recorder_id} keepalive-time={self.keepalive_time} ! imxvideoconvert_g2d ! v4l2h264enc ! h264parse ! queue ! rtph264pay name=pay0 pt=96 >>/dev/null 2>&1\""
            self.rtsp_process = subprocess.Popen(record_cmd, shell=True)
            print (f"Start recording as RTSP server. Play URI: rtsp://{server_ip}:8554/test")
            return Ret.OK
        else:
            print ("ERROR: failed to start recording as RTSP server")
            return Ret.ERROR

    def stop (self):
        if self.rtsp_process != "":
            self.rtsp_process.terminate()
        print ("Stop recording as a RTSP server")

        # Clear out the remaining files
        if self.rtsp_server_is_exist(self.rtsp_server) == True:
            self.clear_file (os.getcwd(), f"{self.rtsp_server}")

# Record screen data to RTP stream
class RtpRecorder (ScreenRecorder):
    rtp_process = ""

    def start (self, device, receiver_ip):
        if super().start(device) == Ret.OK:
            # Start recording 
            server_ip = self.get_local_ip()
            record_cmd = f"gst-launch-1.0 pipewiresrc path={self.recorder_id} keepalive-time={self.keepalive_time} ! imxvideoconvert_g2d ! v4l2h264enc extra-controls=\"encode, video_bitrate=10000000\" ! h264parse ! queue ! mpegtsmux ! rtpmp2tpay ! udpsink host={receiver_ip} port=1234 sync=false async=false >>/dev/null 2>&1"
            self.rtp_process = subprocess.Popen(record_cmd, shell=True)
            print (f"Start recording as RTP server. Play URI: rtp://@{receiver_ip}:1234")
            return Ret.OK
        else:
            print ("ERROR: failed to start recording as RTP server")
            return Ret.ERROR

    def stop (self):
        if self.rtp_process != "":
            self.rtp_process.terminate()
        print ("Stop recording as a RTP server")

if __name__ == "__main__":
    arg_parser = argparse.ArgumentParser(description='Start screen recording on i.MX95 board')
    arg_parser.add_argument('--rtsp_record', type=int, default=0, help='record as RTSP server. 1:enable(by default), 0:disable')
    arg_parser.add_argument('--hls_record', type=int, default=1, help='record as HLS live server. 1:enable, 0:disable(by default)')
    arg_parser.add_argument('--file_record', type=int, default=0, help='record to file. 1:enable, 0:disable(by default)')
    arg_parser.add_argument('--rtp_record', type=str, default="", help='record as RTP server. Need to enter receiver ip')
    arg_parser.add_argument('--restore_params', type=int, default=0, help='restore weston parameters when the script exists')
    arg_parser.add_argument('-v', '--verbose', action='store_true', help='detail information')

    print (VERSION_STR)
    args = arg_parser.parse_args()
    ret = Ret.OK
    if (args.rtsp_record == 0 and args.hls_record == 0 and args.file_record == 0 and args.rtp_record == ""):
        print ("ERROR: no function is enabled, please check the input parameters!")
        ret = Ret.ERROR

    if (ret == Ret.OK):
        # Check and enable pipewire service if needed
        pipewire_server = PipewireServer()
        if pipewire_server.is_running() == False:
            pipewire_server.start()
            print ("Start pipewire")

        # Enable pipewire backend
        pipewire_backend = PipewireBackend()
        ret = Ret.ERROR
        if args.rtsp_record:
            rtsp_record = RtspRecorder()
            ret = rtsp_record.start(pipewire_backend)
        if args.hls_record:
            hls_record = HlsRecorder()
            if hls_record.start(pipewire_backend) == Ret.OK:
                ret = Ret.OK
        if args.file_record:
            file_record = FileRecorder()
            if file_record.start(pipewire_backend) == Ret.OK:
                ret = Ret.OK
        if args.rtp_record != "":
            rtp_record = RtpRecorder()
            if rtp_record.start(pipewire_backend, args.rtp_record) == Ret.OK:
                ret = Ret.OK

        try:
            if ret == Ret.OK:
                while True:
                    time.sleep (1)
        except KeyboardInterrupt:
            pass
        finally:
            if args.rtsp_record:
                rtsp_record.stop()
            if args.hls_record:
                hls_record.stop()
            if args.file_record:
                file_record.stop()
            if args.rtp_record != "":
                rtp_record.stop()

            if args.restore_params:
                pipewire_backend.disable()

    print("Exit pipewire recorder program\n")
    sys.exit(0)
