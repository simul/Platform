import os
import sys
import getopt
from pathlib import Path
import fnmatch
import subprocess
import configparser
from enum import Enum

class PlatformType(Enum):
	UNKNOWN = 0
	PS4 = 1
	PS5 = 2
	XBOX_ONE = 3
	XBOX_ONE_GDK = 4
	XBOX_SERIES = 5
	WIN_GDK = 6

def GetPlatformType(PlatformName):
	platform = PlatformType.UNKNOWN
	PlatformName = PlatformName.upper()

	# Ordered as:
	# Simul Platform name, VS Build name and then Alternative names.
	if PlatformName == "PS4" or PlatformName == "ORBIS": 
		platform = PlatformType.PS4
	elif PlatformName == "COMMODORE" or PlatformName == "PROSPERO" or PlatformName == "PS5":
		platform = PlatformType.PS5
	elif PlatformName == "XBOXONE" or PlatformName == "DURANGO" or PlatformName == "XBOXONES" or PlatformName == "XBOXONEX": 
		platform = PlatformType.XBOX_ONE
	elif PlatformName == "XBOXONEGDK" or PlatformName == "GAMING.XBOX.XBOXONE.X64" or PlatformName == "XBOXSERIESSGDK" or PlatformName == "XBOXSERIESXGDK":
		platform = PlatformType.XBOX_ONE_GDK
	elif PlatformName == "SPECTRUM" or PlatformName == "GAMING.XBOX.SCARLETT.X64" or PlatformName == "XBOXSERIESS" or PlatformName == "XBOXSERIESX" or PlatformName == "SCARLETT":
		platform = PlatformType.XBOX_SERIES
	elif PlatformName == "WINGDK" or PlatformName == "GAMING.DESKTOP.X64":
		platform = PlatformType.WIN_GDK
	else:
		platform = PlatformType.UNKNOWN

	return platform

def add_section_header(properties_file, header_name):
	# read a .properties file without section headers.
	# configparser.ConfigParser requires at least one section header in a properties file.
	# Our properties file doesn't have one, so add a header to it on the fly.
	yield '[{}]\n'.format(header_name)
	for line in properties_file:
		yield line

def read_config_file(platform_dir, filename):
	cwd = os.getcwd()
	os.chdir(os.path.abspath(platform_dir))
	config = configparser.RawConfigParser()
	if not os.path.isfile(filename):
		file = open(filename, "w+", encoding="utf_8")
	else:
		file = open(filename, encoding="utf_8")
	config.read_file(add_section_header(file, 'asection'), source=filename)
	os.chdir(cwd)
	return config['asection']

def wrap_dq(str):
	return ("\"" + str + "\"")

def GetMSBuild(version_num):
	if version_num.find(".0") == -1: version_num += ".0"
	
	cwd = os.getcwd()
	VSW=os.environ["ProgramFiles(x86)"] + "\\Microsoft Visual Studio\\Installer\\"
	os.chdir(VSW)
	cmd = "vswhere -version " + version_num + " -requires Microsoft.Component.MSBuild -find MSBuild\\**\\Bin\\MSBuild.exe"
	process = subprocess.Popen(cmd, stdout=subprocess.PIPE, encoding="utf8")
	
	result = ""
	for line in iter(process.stdout.readline, ""):
		path = line.strip()
		if not path: 
			break
		else:
			b_2017 = path.find("2017") != -1
			b_2019 = path.find("2019") != -1
			if version_num == "15.0" and b_2017:
				result = path
				break
			elif version_num == "16.0" and b_2019: 
				result = path
				break
			else: 
				continue
	
	if not path: 
		print("ERROR: No path to MSBuild.exe that matches version " + version_num + " was found.")

	process.poll()
	os.chdir(cwd)
	return result
	
def RunCMake(src_dir, build_dir, generator_name, platform_name, defines):
	if not src_dir or not build_dir or not generator_name:
		print("ERROR: Invalid source/build directories or generator name")
		print("Source Directory: ", src_dir)
		print("Build Directory: ", build_dir)
		print("Source Directory: ", generator_name)
		return

	Path(build_dir).mkdir(True, exist_ok=True)

	cmd = "cmake"
	cmd += " -S " + src_dir
	cmd += " -B " + build_dir
	cmd += " -G " + wrap_dq(generator_name)
	if platform_name:
		cmd += " -A " + platform_name
	
	for define in defines:
		cmd += " " + define

	print(cmd)
	returnCode = subprocess.call(cmd, stderr=subprocess.STDOUT,  shell=True)
	print("CMake.exe has exited with code %d (0x%x).", returnCode, returnCode)

def RunMSBuild(MSBuild, build_dir, platform_name):
	cwd = os.getcwd()
	os.chdir(os.path.abspath(build_dir))

	sln = ""
	for root, dirs, files in os.walk("."):
		if sln: break
		for name in files:
			if fnmatch.fnmatch(name, "*.sln"):
				sln = name
				break

	if not sln:
		print("ERROR: Unable to find .sln file in build directory: " + build_dir)
		return

	cmd = wrap_dq(MSBuild) + " " + sln + " /m:1 /p:Configuration=Release /p:Platform=" + platform_name

	print(cmd)
	returnCode = subprocess.call(cmd, stderr=subprocess.STDOUT,  shell=True)
	print("MSBuild.exe has exited with code %d (0x%x).", returnCode, returnCode)
	os.chdir(cwd)

def main(argv):
	cwd = os.getcwd()

	#Pass cmd args
	PlatformName=""
	BuildDir=""
	opts, args = getopt.getopt(argv, "hp:b:")
	for opt, arg in opts:
		if opt == "-h":
			print("usage: BuildConsole.py -p <Platform> -b <BuildDir>")
			sys.exit(0)
		elif opt in "-p":
			PlatformName = arg
		elif opt in "-b":
			BuildDir = arg
		else:
			continue

	#Assert and finalise PlatformName
	if not PlatformName:
		print("ERROR: Unable to ascertain Platfom from command line arguments.")
		sys.exit(0)
	else:
		PlatformName=PlatformName.strip()

	ePlatform = GetPlatformType(PlatformName)
	
	if ePlatform == PlatformType.UNKNOWN:
		print("ERROR: Unknown Console type.")
		sys.exit(0)

	#Set inmplicit BuildDir
	if not BuildDir:
		BuildDir = "build_" + PlatformName
	else:
		BuildDir=BuildDir.strip()

	#Get ThisPlatformProperties info
	ThisPlatformPropertiesFile = "ThisPlatform.properties"
	if ePlatform == PlatformType.XBOX_ONE_GDK:
		PlatformName = "XboxOne"
		ThisPlatformPropertiesFile = "ThisPlatformGDK.properties"
	if ePlatform == PlatformType.WIN_GDK:
		PlatformName = "Windows"
		ThisPlatformPropertiesFile = "ThisPlatformGDK.properties"
	
	ThisPlatformProperties = read_config_file(PlatformName, ThisPlatformPropertiesFile)
	PLATFORM_NAME = ThisPlatformProperties.get("PLATFORM_NAME")
	PLATFORM_NAME_ALT = ThisPlatformProperties.get("PLATFORM_NAME_ALT")
	PLATFORM_NAME_VS = ThisPlatformProperties.get("PLATFORM_NAME_VS")
	VISUAL_STUDIO_VERSION = ThisPlatformProperties.get("VISUAL_STUDIO_VERSION")
	CMAKE_TOOLCHAIN = ThisPlatformProperties.get("CMAKE_TOOLCHAIN")
	SDK_DIR = ThisPlatformProperties.get("SDK_DIR")
	SDK_VERSION = ThisPlatformProperties.get("SDK_VERSION")

	#Check VISUAL_STUDIO_VERSION
	vs_ver_list = VISUAL_STUDIO_VERSION.split()
	vs_ver_list_is_ok = False
	if len(vs_ver_list):
		if vs_ver_list[0] == "Visual" and vs_ver_list[1] == "Studio" and len(vs_ver_list) == 4:
			vs_ver_list_is_ok = True
			
	if not vs_ver_list_is_ok:
		print("ERROR: Property VISUAL_STUDIO_VERSION defined in " + PlatformName + "/" + ThisPlatformPropertiesFile + "is not in the form \"Visual Studio <VER> <YEAR>\".")
		print("Provided value: " +  VISUAL_STUDIO_VERSION)
		sys.exit(0)

	#Get MSBuild path
	MSBuild = GetMSBuild(vs_ver_list[2])

	#Set CMake PlatformName (-A parameter)
	CMake_A_PlatformName = ""
	if ePlatform == PlatformType.PS4 or ePlatform == PlatformType.PS5:
		CMake_A_PlatformName = PLATFORM_NAME_VS

	#Set CMake defines
	defines=[]
	defines.append("-D CMAKE_BUILD_TYPE=Debug,Release")
	defines.append("-D CMAKE_TOOLCHAIN_FILE=" + wrap_dq(cwd+"\\"+PlatformName+"\\"+CMAKE_TOOLCHAIN))
	defines.append("-D SIMUL_SFX_EXECUTABLE=" + wrap_dq(cwd+"\\build_x64\\bin\\Release\\Sfx.exe"))
	
	if ePlatform == PlatformType.PS4:
		defines.append("-D "+ wrap_dq("SCE_ORBIS_SDK_DIR="+SDK_DIR+"\\"+SDK_VERSION))
	if ePlatform == PlatformType.PS5:
		defines.append("-D " + wrap_dq("COMMODORE_SDK_DIR="+SDK_DIR+"\\"+SDK_VERSION))
	if ePlatform == PlatformType.XBOX_ONE:
		defines.append("-D REQUIRED_XB1_TOOLCHAIN_VERSION="+SDK_VERSION)
	if ePlatform == PlatformType.XBOX_SERIES or ePlatform == PlatformType.XBOX_ONE_GDK or PlatformType.WIN_GDK:
		defines.append("-D REQUIRED_GDK_TOOLCHAIN_VERSION="+SDK_VERSION)
	
	defines.append("-D SIMUL_SOURCE_BUILD=1")
	defines.append("-D CMAKE_BUILD_TOOL=" + wrap_dq(MSBuild))
	defines.append("-D CMAKE_MAKE_PROGRAM=" + wrap_dq(MSBuild))

	RunCMake(cwd, BuildDir, VISUAL_STUDIO_VERSION, CMake_A_PlatformName, defines)
	RunMSBuild(MSBuild, BuildDir, PLATFORM_NAME_VS)

if __name__ == "__main__":
	main(sys.argv[1:])