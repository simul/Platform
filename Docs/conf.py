# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
# import os
# import sys
# sys.path.insert(0, os.path.abspath('.'))
import subprocess, os, glob

def configureDoxyfile(input_dir, output_dir):
	with open('Doxyfile.in', 'r') as file :
		filedata = file.read()

	filedata = filedata.replace('@DOXYGEN_INPUT_DIR@', input_dir)
	filedata = filedata.replace('@DOXYGEN_OUTPUT_DIR@', output_dir)
	filedata = filedata.replace('@DOXYGEN_SEARCH_PATH@', '..')
	
	with open('Doxyfile', 'w') as file:
		file.write(filedata)
		print(filedata)


def generate_doxygen_xml(app):
	# Check if we're running on Read the Docs' servers
	read_the_docs_build = os.environ.get('READTHEDOCS', None) == 'True'
	print("read_the_docs_build: "+str(read_the_docs_build))
	breathe_projects = {}
	if read_the_docs_build:
		for file in glob.glob("/../Math/*.*",recursive=True):
			print(file)
		input_dir = '"../Math","../Core","../CrossPlatform"'
		output_dir = '../build_docs/Docs/doxygen'
		if not os.path.exists(output_dir):
			os.makedirs(output_dir)
		configureDoxyfile(input_dir, output_dir)
		print('Executing doxygen at '+os.getcwd())
		retcode=subprocess.call('doxygen', shell=True)
		if retcode < 0:
			sys.stderr.write("doxygen terminated by signal %s" % (-retcode))
		breathe_projects['Platform'] = output_dir + '/xml'
		for file in glob.glob(output_dir+"/xml/*.*",recursive=True):
			print(file)

# -- Project information -----------------------------------------------------

project = 'Platform'
copyright = '2007-2023, Simul Software Ltd'
author = 'Roderick Kennedy'


# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [ "breathe","myst_parser",'sphinx.ext.autosectionlabel']
source_suffix = {
	'.rst': 'restructuredtext',
}
# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ["**/README.md","**/Readme.md","ReadMe.md","**/*.md","External/**/*.*"]


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'platform_sphinx_theme'
html_theme_path = ["."]


# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']

breathe_default_project = "Platform"
master_doc = 'index'


def setup(app):
    # Add hook for building doxygen xml when needed
    app.connect("builder-inited", generate_doxygen_xml)