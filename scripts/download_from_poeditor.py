#!/usr/bin/env python3

from poeditor import POEditorAPI
import os
import subprocess
import argparse

project_id = "669797"
filename = "venus"


def getGitRoot():
    return subprocess.Popen(['git', 'rev-parse', '--show-toplevel'], stdout=subprocess.PIPE).communicate()[0].rstrip().decode('utf-8')


parser = argparse.ArgumentParser()
parser.add_argument('-p', '--project', nargs=1, help='project id (default: '+project_id+')')
parser.add_argument('-f', '--filename', nargs=1, help='filename prefix (default: '+filename+')')
parser.add_argument('-l', '--languages', action='append', help='the code of the language(s) to download (default: all)')
args = parser.parse_args()

if args.project:
    project_id = args.project[0]
if args.filename:
    filename = args.filename[0]

# we need to get the API token from the environment
try:
    os.environ['POEDITOR_TOKEN']
except:
    raise SystemExit("### Please set POEDITOR_TOKEN environment variable")

client = POEditorAPI(os.environ.get('POEDITOR_TOKEN', None))

# Fetch the languages for this project
project = client.view_project_details(project_id)

for language in client.list_project_languages(project_id):
    if args.languages and not language["code"] in args.languages:
        # skip language
        continue

    print("Language: %s (%s)" % (language["name"], language["code"]))

    # Skip English as that is the source
    if language["name"] == 'english':
        print("Skipping English as that is the source language")
        continue

    ts_file = "%s/translations/%s_%s.ts" % (getGitRoot(), filename, language["code"])
    print("- exporting ts file [%s]" % ts_file)
    client.export(project_id, language["code"], 'ts', local_file=ts_file)

here = os.path.dirname(__file__)
