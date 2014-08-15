
import argparse
import os
import subprocess
import sys
import json
import zipfile
from pprint import pprint

# vogl_analysis.py overview
# Initially this script automates frag shader performance analysis per frame
#   Over time it may grow to include per-draw debug analysis as well as other features
# Here's the current high-level view of how it performs frame perf analysis.
#   1. Take a bin file and specified frame as input (along with various other options)
#   2. Perform dump on bin file and analyze state to identify FSs that are used for given frame
#   3. Run the trace for specified loop count (default 100) on specified frame and record FPS as baseline
#   4. Foreach FS used in the frame:
#      a. Null out FS (make it output constant color)
#      b. Run trace for loop count over given frame
#      c. Record FPS w/ this FS NULL
#   5. Report out FPS vs. baseline for each null FS


def handle_args():
    parser = argparse.ArgumentParser(description='Perform analysis of vogl trace.')
    parser.add_argument('trace_bin_file', help='The VOGL .bin trace file on which to perform analysis.')
    parser.add_argument('--vogl_path', required=True,  help='Path to vogl executable used to replay trace.')
    parser.add_argument('--fs_pp_path', required=True,  help='Path to FS pre-processor.')
    parser.add_argument('--vogl_args',  default='--loop_frame 1 --loop_len 1 --benchmark',  help='Arguments to pass along to VOGL. NOTE: "--loop_count" is also added based on that option.')
    parser.add_argument('--loop_count', type=int, default=100, help='Number of times to loop.')
    parser.add_argument('--frame', dest='frame_num', type=int,  help='The frame on which analysis is performed.')
    parser.add_argument('--dump_images', action='store_true', default=False, help='Create "img_dump" dir where bin file is and dump frame images there.')
    parser.add_argument('--rename_images', action='store_true', default=False, help='Renames images in "img_dump" dir to simplify sorting. Must also specify "--dump_images".')
    parser.add_argument('--dump_shaders', action='store_true', default=False, help='Create "shader_dump" dir where bin file is and dump input & output shaders there.')
    parser.add_argument('--debug_quit_after_num', type=int, default=0, help='Debug option, provide an int and and testing will quit after that many NULL FSs.')
    return parser.parse_args()

def print_stats(stat_dict, time_dict):
    # sort results to identify FS usage
    print("Baseline FPS: %s" % time_dict['baseline_fps'])
    print("Baseline Time: %s" % time_dict['baseline_time'])
    for fps in sorted(time_dict['fps'],  reverse=True):
        for fs in sorted(time_dict['fps'][fps]):
            print("Ran at %s fps in %ss w/ FS#%s NULL which is %.4f percent of baseline FPS" % (fps, stat_dict["shader_dict"][fs]["loop_time"], fs, float(fps)/float(time_dict['baseline_fps'])))

def find_index(in_list,  str_to_match):
    index = 0
    for item in in_list:
        if str_to_match in item:
            return index
        index += 1
    return -1

# This is a convenience function to rename image files so they're easier to sort through
#  By default frame number is at the end of image name and this function puts it at the
#  front so that all images from same frame will be grouped together
# This function will also delete images that aren't for the frame of interest
def rename_images(opts, img_dir):
    frame_str = "%07d" % opts.frame_num;
    # loop through files in img_dir, renaming frames of interest and deleting others
    for f in os.listdir(img_dir):
        if frame_str in f:
            # rename this file
            new_name = "%s_%s" % (frame_str,  f.replace("_%s" % frame_str,  ''))
            #print("Renaming %s to %s" % (f,  new_name))
            os.rename(os.path.join(img_dir, f), os.path.join(img_dir, new_name))
        else:
            # delete this file
            if os.path.isfile(os.path.join(img_dir, f)):
                #print("Removing file %s" % os.path.join(img_dir, f))
                os.remove(os.path.join(img_dir, f))

# Run the trace with the given options
#  Do initial baseline run and then re-run with each shader substituted to NULL
#  If dumping images is requested, do that in a separate run to avoid affecting perf numbers on looping
def run_trace(opts,  stat_dict):
    count = 0
    time_dict = {}
    time_dict['fps'] = {}
    time_dict['time'] = {}
    # First get baseline time w/o any null shaders
    if opts.dump_images:
        img_dir = os.path.join(os.path.dirname(opts.trace_bin_file),  "dump_imgs")
        if not os.path.exists(img_dir):
            os.mkdir(img_dir)
        img_prefix = os.path.join(img_dir,  "default")
        print("\tWill dump frame images to '%s'" % img_dir)
        print("\tDumping default image for frame %i" % (opts.frame_num))
        cmd_string = "%s replay --dump_screenshots --dump_screenshots_prefix %s %s" % (opts.vogl_path, img_prefix, opts.trace_bin_file)
        p = subprocess.Popen(cmd_string.split(),  stdout=subprocess.PIPE)
        out,  err = p.communicate()
    cmd_string = "%s replay %s --loop_count %s %s" % (opts.vogl_path, opts.vogl_args, opts.loop_count,  opts.trace_bin_file)
    p = subprocess.Popen(cmd_string.split(),  stdout=subprocess.PIPE)
    out,  err = p.communicate()
    out_list = out.split()
    time_dict['baseline_fps'] = out_list[find_index(out_list, "Looping:")+6]
    time_dict['baseline_time'] = out_list[find_index(out_list, "Looping:")+4]
    #print("Baseline FPS: %s" % baseline_fps)
    for fs in stat_dict["shader_dict"]:
        print("\tRunning with FS #%s NULL'd out" % (fs))
        print("\t\tFS #%s is used in %s draws w/ %s total tris" % (fs, stat_dict["shader_dict"][fs]["num_draws"], stat_dict["shader_dict"][fs]["num_tris"]))
        if opts.dump_images:
            img_prefix = os.path.join(img_dir,  "null_fs%s" % (fs))
            print("\tDumping image w/ NULL FS#%s for frame %i" % (fs, opts.frame_num))
            cmd_string = "%s replay --dump_screenshots --dump_screenshots_prefix %s --fs_preprocessor %s --fs_preprocessor_options NULL_FS[%s] %s" % (opts.vogl_path, img_prefix, opts.fs_pp_path, fs, opts.trace_bin_file)
            p = subprocess.Popen(cmd_string.split(),  stdout=subprocess.PIPE)
            out,  err = p.communicate()
        cmd_string = "%s replay %s --loop_count %s --fs_preprocessor %s --fs_preprocessor_options NULL_FS[%s] %s" % (opts.vogl_path, opts.vogl_args, opts.loop_count, opts.fs_pp_path, fs, opts.trace_bin_file)
        if opts.dump_shaders:
            shader_dir = os.path.join(os.path.dirname(opts.trace_bin_file),  "dump_shaders")
            if not os.path.exists(shader_dir):
                os.mkdir(shader_dir)
            shader_prefix = os.path.join(shader_dir,  "analysis_")
            cmd_string += " --fs_preprocessor_prefix %s" % shader_prefix
        cmd_list = cmd_string.split()
        #print(" ".join(cmd_list))
        p = subprocess.Popen(cmd_list,  stdout=subprocess.PIPE)
        out,  err = p.communicate()
        #print(out)
        # parse out loop time and store in stats
        out_list = out.split()
        loop_index = find_index(out_list,  "Looping:")
        if loop_index < 0:
            print("WARN: Bad output data for NULL FS#%s, did something go wrong?" % fs)
        else:
            loop_time = out_list[loop_index+4]
            loop_fps = out_list[loop_index+6]
            #print("Loop FPS: %s" % loop_fps)
            stat_dict["shader_dict"][fs]["loop_time"] = loop_time
            stat_dict["shader_dict"][fs]["loop_fps"] = loop_fps
            # Store a list of FSs per time/fps in case 2+ FSs hit same value
            if loop_fps in time_dict['fps']: #append to existing list
                time_dict['fps'][loop_fps].append(fs)
            else: #create new list
                time_dict['fps'][loop_fps] = [fs, ]
            if loop_time in time_dict['time']:
                time_dict['time'][loop_time].append(fs)
            else:
                time_dict['time'][loop_time] = [fs, ]
        count += 1
        if opts.debug_quit_after_num > 0 and count > opts.debug_quit_after_num:
            print("Option '--debug_quit_after_num %i' given so quitting now." % opts.debug_quit_after_num)
            break;
    if opts.dump_images and opts.rename_images:
        rename_images(opts, img_dir)
    return (stat_dict,  time_dict)

# Having difficulty with json parsing so just parsing file line-by-line for now
def parse_shader_state_file(state_file):
    shader_dict = {}
    parse_prog = False
    grab_prog_handle = False
    grab_shader_type = False
    grab_shader_handle = False
    prog_handle = 0
    shader_type = "NULL"
    shader_handle = 0
    with open(state_file) as in_file:
        for line in in_file:
            if parse_prog:
                if 'link_time_snapshot' in line:
                    grab_prog_handle = True
                if 'handle' in line:
                    if grab_prog_handle:
                        #print("grab_prog_handle")
                        prog_handle = line[line.find(':')+2:line.find(',')]
                        shader_dict[prog_handle] = {}
                        #pprint(shader_dict)
                        grab_prog_handle = False
                    elif grab_shader_handle:
                        #print("grab_shader_handle")
                        shader_handle = line[line.find(':')+2:line.find(',')]
                        grab_shader_type = True
                if 'type' in line and grab_shader_type:
                    #print("grab_shader_type")
                    shader_type = line[line.find(':')+3:line.find(',')-1]
                    shader_dict[prog_handle][shader_type] = shader_handle
                    #pprint(shader_dict)
                    grab_shader_type = False
                if 'shader_objects' in line:
                    grab_shader_handle = True
                if '],' in line and grab_shader_handle:
                    grab_shader_handle = False
            if 'Program' in line:
                #print("Found Program")
                parse_prog = True
    #pprint(shader_dict)
    return shader_dict

def parse_frame_file(frame_file,  shader_program_dict):
    stat_dict = {}
    stat_dict["shader_dict"] = {}
    # alternate way to store data for simplified sorting by time values
    stat_dict["results"] = {}
    stat_dict["results"]["fps"] = {}
    stat_dict["results"]["times"] = {}
    total_time = 0.0
    cur_program = "0x0"
    with open(frame_file) as in_file:
        frame_data = json.load(in_file)
        #print("Analyzing frame %i" % frame_data["meta"]["cur_frame"])
        for element in frame_data["packets"]:
            #pprint(element)
            if 'glUseProgram' in element["func"]:
                cur_program = str(int(element["params"]["program"],  0))
            if 'glDraw' in element["func"]:
                if "count" in element["params"]:
                    #print("gl Call #%i is draw of %i %s w/ Program %s" % (element["call_counter"], element["params"]["count"], element["params"]["mode"], cur_program))
                    if cur_program in shader_program_dict:
                        #print("\tProg %s uses FS %s & VS %s" % (cur_program,  shader_program_dict[cur_program]["GL_FRAGMENT_SHADER"],  shader_program_dict[cur_program]["GL_VERTEX_SHADER"]))
                        if shader_program_dict[cur_program]["GL_FRAGMENT_SHADER"] not in stat_dict["shader_dict"]:
                            stat_dict["shader_dict"][shader_program_dict[cur_program]["GL_FRAGMENT_SHADER"]] = {}
                            stat_dict["shader_dict"][shader_program_dict[cur_program]["GL_FRAGMENT_SHADER"]]["num_draws"] = 0
                            stat_dict["shader_dict"][shader_program_dict[cur_program]["GL_FRAGMENT_SHADER"]]["num_tris"] = 0
                            stat_dict["shader_dict"][shader_program_dict[cur_program]["GL_FRAGMENT_SHADER"]]["tot_time"] = 0.0
                        stat_dict["shader_dict"][shader_program_dict[cur_program]["GL_FRAGMENT_SHADER"]]["num_draws"] += 1
                        stat_dict["shader_dict"][shader_program_dict[cur_program]["GL_FRAGMENT_SHADER"]]["num_tris"] += element["params"]["count"]
                    else:
                        print("\tWARN: Don't have Prog %s in shader_prog_dict!" % (cur_program))
                else:
                    pass
                    #print("\tgl Call #%i is type %s w/o a count" % (element["call_counter"], element["func"]))
    #pprint(stat_dict)
    return stat_dict, total_time

# identify and return state snapshot and frame api files
def find_api_and_state_json_files(dir, prefix,  frame_num):
    frame_api_name = "%s_%06d.json" % (prefix,  int(frame_num))
    frame_api_file = os.path.join(dir,  frame_api_name)
    if not os.path.exists(frame_api_file):
        print("ERROR: Can't find API dump file %s" % frame_api_file)
    for f in os.listdir(dir):
        if 'state_snapshot' in f and 'json' in f:
            ss_json = os.path.join(dir,  f)
            return (frame_api_file,  ss_json)

def run_vogl_dump(opts):
    # check if dump dir exists
    bin_path = os.path.dirname(opts.trace_bin_file)
    bin_dump_path = os.path.join(bin_path,  "dump")
    prefix = "vogl_analysis"
    if (os.path.exists(bin_dump_path)):
        print("\tFYI: Path '%s' already exists so not running VOGL dump. To force a re-dump of trace data, please remove or rename that dir." % bin_dump_path)
        return find_api_and_state_json_files(bin_dump_path,  prefix, opts.frame_num)
    # run vogl w/ dump, outputting to dump dir
    os.mkdir(bin_dump_path)
    dump_prefix = os.path.join(bin_dump_path,  prefix)
    vogl_dump_cmd = "%s dump %s %s" % (opts.vogl_path, opts.trace_bin_file,  dump_prefix)
    print("Dump cmd: %s" % vogl_dump_cmd)
    p = subprocess.Popen(vogl_dump_cmd.split(),  stdout=subprocess.PIPE)
    out,  err = p.communicate()
    # identify zip file and extract it to dump dir
    for f in os.listdir(bin_dump_path):
        if '_trace_archive' in f and 'zip' in f:
            print("Found zip file: %s" % f)
            full_zip_path = os.path.join(bin_dump_path,  f)
            zf = zipfile.ZipFile(full_zip_path)
            zf.extractall(bin_dump_path)
            
    return find_api_and_state_json_files(bin_dump_path,  prefix, opts.frame_num)

def main(argv=None):
    opts = handle_args()
    #print(opts)
    print("STEP1: Run trace in dump mode.")
    (trace_json,  state_json) = run_vogl_dump(opts)
    print("STEP2: Parsing state json file to identify FSs per program.")
    shader_prog_dict = parse_shader_state_file(state_json)
    print("STEP3: Parsing frame #%i to identify which FSs are used." % (opts.frame_num))
    stat_dict, time = parse_frame_file(trace_json,  shader_prog_dict)
    print("STEP4: Run trace with each FS null'd out, looping %i times on frame %i." % (opts.loop_count, opts.frame_num))
    stat_dict, time_dict = run_trace(opts,  stat_dict)
    print("STEP5: Print stats")
    print_stats(stat_dict, time_dict)

if __name__ == "__main__":
    sys.exit(main())
