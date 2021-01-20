import os
import shlex, subprocess
import sys
import configparser
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
	cmakeCmd = ["cmake.exe", '-G','Visual Studio 16 2019', os.path.relpath(src, build_path)]
	retCode = subprocess.check_call(cmakeCmd+flags, stderr=subprocess.STDOUT, shell=True)
	sln=find('*.sln','.')[0]
	#print(MSBUILD+'/p:Configuration=Release'+'/p:Platform=x64'+sln)
	pid=subprocess.Popen([MSBUILD,'/p:Configuration=Release','/p:Platform=x64',sln])
	os.chdir(wd)

def GetMSBuild():
	VSW=os.environ['ProgramFiles(x86)']+'/Microsoft Visual Studio/Installer/vswhere.exe'
	process = subprocess.Popen([VSW,'-latest','-requires','Microsoft.Component.MSBuild','-find','MSBuild\\**\\Bin\\MSBuild.exe'], stdout=subprocess.PIPE)
	MSB = process.stdout.readline().strip()
	process.poll()
	return MSB

def execute():
	repo = Repo(os.getcwd())
	#We can update from the main repo; doesn't require credentials in submodules
	repo.git.submodule('update', '--init') 
	#sms = repo.submodules
	#for sm in sms:
	#	sm.update()

	glfwflags=["-DGLFW_BUILD_DOCS=false","-DGLFW_BUILD_EXAMPLES=false","-DGLFW_BUILD_TESTS=false","-DGLFW_INSTALL=false","-DCMAKE_C_FLAGS_DEBUG=/MTd /Zi /Ob0 /Od /RTC1","-DCMAKE_C_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG","-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=../lib"]
	cmake('External/glfw','External/glfw/build_md',glfwflags)
	cmake('External/glfw','External/glfw/build_mt',glfwflags+["-DUSE_MSVC_RUNTIME_LIBRARY_DLL=false"])
	platform_flags=[]
	cmake('.','build',platform_flags)


version=read_config_file('version.properties')
user=read_config_file('user.properties')
MSBUILD=user.get('MSBUILD',GetMSBuild())
execute()