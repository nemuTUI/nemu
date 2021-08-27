#!/usr/bin/env python3

from argparse import ArgumentParser, Namespace
from subprocess import run
from json import dumps
from xml.etree.ElementTree import fromstring as xml_parse, Element
from typing import List
from sys import exit
from os import path

import traceback
import logging


def sarif_template(codePath: str, uriBaseId: str) -> dict:
    repoUri = git_remote_url(codePath)
    codeRev = git_rev(codePath)
    codeBranch = git_branch(codePath)

    return {
        "$schema": "https://raw.githubusercontent.com/oasis-tcs/sarif-spec/master/Schemata/sarif-schema-2.1.0.json",
        "version": "2.1.0",
        "runs": [
            {
                "tool": {
                    "driver": {
                        "name": "CppCheck",
                        "informationUri": "http://cppcheck.sourceforge.net",
                    },
                },
                "originalUriBaseIds": {
                    uriBaseId: {
                        "uri": f"FILE://{path.abspath(codePath)}/",
                    },
                },
                "versionControlProvenance": [
                    {
                        "repositoryUri": repoUri,
                        "revisionId": codeRev,
                        "branch": codeBranch,
                        "mappedTo": {
                            "uriBaseId": uriBaseId,
                        },
                    },
                ],
                "artifacts": [],
                "results": [],
            },
        ],
    }


def git_rev(codePath: str) -> str:
    return run(
        ["git", "-C", codePath, "rev-parse", "HEAD"],
        capture_output=True
    ).stdout.decode().strip()


def git_branch(codePath: str) -> str:
    return run(
        ["git", "-C", codePath, "branch", "--show-current"],
        capture_output=True
    ).stdout.decode().strip()


def git_remote_url(codePath: str) -> str:
    return run(
        ["git", "-C", codePath, "remote", "get-url", "origin"],
        capture_output=True
    ).stdout.decode().strip()


def run_cppcheck(
    cppcheck: str, args: List[str], codePath: str
) -> Element:
    out = run(
        [cppcheck] + args + [codePath],
        capture_output=True
    ).stderr.decode()

    tree = xml_parse(out)
    return(tree)


def parse_error(error: Element, uriBaseId: str, codePath: str) -> dict:
    id = f"CWE{error.attrib['cwe']}"
    name = id_to_name(error.attrib["id"])
    text = error.attrib["verbose"]
    severity = error.attrib["severity"]
    file = path.relpath(error[0].attrib["file"], codePath)
    line = error[0].attrib["line"]

    return {
        "ruleId": id,
        "name": name,
        "message": {
            "text": text,
        },
        "properties": {
            "Severity": severity,
        },
        "locations": [
            {
                "physicalLocation": {
                    "artifactLocation": {
                        "uri": file,
                        "uriBaseId": uriBaseId,
                    },
                    "region": {
                        "startLine": line,
                    },
                },
            },
        ],
    }


def id_to_name(string: str) -> str:
    words = [[string[0].upper()]]

    for c in string[1:]:
        if words[-1][-1].islower() and c.isupper():
            words.append(list(c.lower()))
        else:
            words[-1].append(c)

    return ' '.join([''.join(word) for word in words])


def main(args: Namespace):
    cppcheck = args.cppcheck
    cppcheckArgs = args.cppcheck_args.split(" ")
    codePath = args.path
    uriBaseId = args.base_uri
    outputPath = args.output

    xml = run_cppcheck(cppcheck, cppcheckArgs, codePath)

    cppcheckVersion = xml[0].attrib["version"]
    errors = xml[1]

    sarif = sarif_template(codePath, uriBaseId)

    sarif["runs"][0]["tool"]["driver"]["version"] = cppcheckVersion

    for error in errors:
        sarif["runs"][0]["results"].append(
            parse_error(error, uriBaseId, codePath)
        )

    if outputPath:
        with open(outputPath, "w") as outputFile:
            outputFile.write(dumps(sarif, indent=2))
    else:
        print(dumps(sarif, indent=2))


if __name__ == "__main__":
    argParser = ArgumentParser()
    argParser.add_argument(
        "path",
        help="Path to project", type=str
    )
    argParser.add_argument(
        "-c", "--cppcheck",
        help="cppcheck command to run", type=str, default="cppcheck"
    )
    argParser.add_argument(
        "-a", "--cppcheck-args",
        help="cppcheck arguments", type=str,
        default="--xml -q"
    )
    argParser.add_argument(
        "-u", "--base-uri",
        help="uriBaseId for SARIF", type=str, default="SRCROOT"
    )
    argParser.add_argument(
        "-o", "--output",
        help="Path to output SARIF file", type=str, default=""
    )

    try:
        main(argParser.parse_args())
    except Exception:
        logging.error(traceback.format_exc())
        exit(1)
