import os
import shlex, subprocess
import sys
import configparser
import pkg_resources
import distutils.core

required = {'gitpython'}
installed = {pkg.key for pkg in pkg_resources.working_set}
missing = required - installed

if missing:
    python = sys.executable
    subprocess.check_call([python, '-m', 'pip', 'install', *missing], stdout=subprocess.DEVNULL)


from pathlib import Path
import os, fnmatch
from git import Repo

def find(pattern, path):
    result = []
    for root, dirs, files in os.walk(path):
        for name in files:
            if fnmatch.fnmatch(name, pattern):
                result.append(name)
    return result

#read a .properties file without section headers.
def add_section_header(properties_file, header_name):
	# configparser.ConfigParser requires at least one section header in a properties file.
	# Our properties file doesn't have one, so add a header to it on the fly.
	yield '[{}]\n'.format(header_name)
	for line in properties_file:
		yield line

def read_config_file(filename):
	config = configparser.RawConfigParser()
	if not os.path.isfile(filename):
		file = open(filename, "w+", encoding="utf_8")
	else:
		file = open(filename, encoding="utf_8")
	config.read_file(add_section_header(file, 'asection'), source=filename)
	return config['asection']

def cmake(src,build_path,flags):
	wd=os.getcwd()
	Path(build_path).mkdir(True,exist_ok=True)
	os.chdir(build_path)
	vs_versions=['Visual Studio 17 2022','Visual Studio 16 2019','Visual Studio 15 2017']
	for vs_version in vs_versions:
		cmakeCmd = ["cmake.exe", '-G',vs_version, os.path.relpath(src, build_path)]
		retCode = subprocess.check_call(cmakeCmd+flags, stderr=subprocess.STDOUT, shell=True)
		if retCode!=0:
			continue
		sln=find('*.sln','.')[0]
		print(MSBUILD+'/p:Configuration=Release'+'/p:Platform=x64'+sln)
		pid=subprocess.Popen([MSBUILD,'/p:Configuration=Release','/p:Platform=x64',sln])
		pid.poll()
		break
	print(MSBUILD+'/p:Configuration=Debug'+'/p:Platform=x64'+sln)
	pid=subprocess.Popen([MSBUILD,'/p:Configuration=Debug','/p:Platform=x64',sln])
	pid.poll()
	#copy outputs if executable e.g. dll's:
	if os.path.exists("bin"):
		main_build=wd+"/build"
		if not os.path.exists(main_build):
			os.makedirs(main_build)
		distutils.dir_util.copy_tree("bin", main_build+"/bin", update=1)

	os.chdir(wd)

def GetMSBuild():
	VSW=os.environ['ProgramFiles(x86)']+'/Microsoft Visual Studio/Installer/vswhere.exe'
	print('VSW '+VSW)
	process = subprocess.Popen([VSW,'-latest','-find','MSBuild\\**\\Bin\\MSBuild.exe'], stdout=subprocess.PIPE)
	#'-requires','Microsoft.Component.MSBuild', not useful.
	MSB = process.stdout.readline().strip().decode('UTF-8')
	process.poll()
	if MSB!="":
		return MSB
	print("vswhere did not find MSBuild, falling back to path search:")
	for root, dirs, files in os.walk(os.environ['ProgramFiles(x86)']):
		if "MSBuild.exe" in files:
			MSB=os.path.join(root, "MSBuild.exe")
	if MSB!="":
		return MSB
	for root, dirs, files in os.walk(os.environ['ProgramFiles']):
		if "MSBuild.exe" in files:
			MSB=os.path.join(root, "MSBuild.exe")
	print('MSB '+MSB)
	return MSB

def execute():
	repo = Repo(os.getcwd())
	#We update only the submodules in "External" - others are private and must be handled manually.
	sms = repo.submodules
	for sm in sms:
		if('External' in sm.module().working_tree_dir):
			print("updating submodule "+sm.name)
			sm.update()

	glfwflags=["-DGLFW_BUILD_DOCS=false","-DGLFW_BUILD_EXAMPLES=false","-DGLFW_BUILD_TESTS=false","-DGLFW_INSTALL=false","-DCMAKE_C_FLAGS_DEBUG=/MTd /Zi /Ob0 /Od /RTC1","-DCMAKE_C_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG","-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=../lib"]
	cmake('External/glfw','External/glfw/build_md',glfwflags)
	cmake('External/glfw','External/glfw/build_mt',glfwflags+["-DUSE_MSVC_RUNTIME_LIBRARY_DLL=false"])
	assimpflags=["-DASSIMP_BUILD_DOCS=false","-DASSIMP_BUILD_EXAMPLES=false","-DASSIMP_BUILD_TESTS=false","-DASSIMP_INSTALL=false","-DCMAKE_C_FLAGS_DEBUG=/MTd /Zi /Ob0 /Od /RTC1","-DCMAKE_C_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG","-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=../lib"]
	cmake('External/assimp','External/assimp/build_md',glfwflags)
	cmake('External/assimp','External/assimp/build_mt',glfwflags+["-DUSE_MSVC_RUNTIME_LIBRARY_DLL=false"])
	platform_flags=[]
	cmake('.','build',platform_flags)


version=read_config_file('version.properties')
user=read_config_file('user.properties')
MSBUILD=user.get('MSBUILD',GetMSBuild())
execute()