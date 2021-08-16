#!/usr/bin/env python3

from argparse import ArgumentParser, Namespace
from subprocess import run
from json import dumps
from typing import List, Dict
from sys import exit
from os import path

import traceback
import logging


def sarif_template(codePath: str, uriBaseId: str, codespell: str) -> dict:
    repoUri = git_remote_url(codePath)
    codeRev = git_rev(codePath)
    codeBranch = git_branch(codePath)
    codespellVersion = codespell_version(codespell)

    return {
        "$schema": "https://raw.githubusercontent.com/oasis-tcs/sarif-spec/master/Schemata/sarif-schema-2.1.0.json",
        "version": "2.1.0",
        "runs": [
            {
                "tool": {
                    "driver": {
                        "name": "codespell",
                        "version": codespellVersion,
                        "informationUri": "https://github.com/codespell-project/codespell",
                        "rules": [
                            {
                                "id": "CS0001",
                                "name": "SpellingMistake",
                                "shortDescription": {
                                    "text": "Probable spelling mistake",
                                },
                                "helpUri": "https://github.com/codespell-project/codespell#readme",
                                "properties": {
                                    "Severity": "style",
                                },
                            },
                        ],
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


def codespell_version(codespell: str) -> str:
    return run(
        [codespell, "--version"],
        capture_output=True
    ).stdout.decode().strip()


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


def run_codespell(
    codespell: str, args: List[str], codePath: str
) -> List[Dict]:
    out = run(
        [codespell] + args + [codePath],
        capture_output=True
    ).stdout.decode().split("\n")

    errors = []
    for str in out:
        if not str:
            continue

        file, line, msg = (x.strip() for x in str.split(":"))
        file = path.relpath(file, codePath)

        errors.append({
            "file": file,
            "line": int(line),
            "msg": msg,
        })
    return errors


def parse_error(error: str, uriBaseId: str) -> dict:
    return {
        "ruleId": "CS0001",
        "ruleIndex": 0,
        "message": {
            "text": f"Possible spelling mistake: {error['msg']}.",
        },
        "locations": [
            {
                "physicalLocation": {
                    "artifactLocation": {
                        "uri": error["file"],
                        "uriBaseId": uriBaseId,
                    },
                    "region": {
                        "startLine": error["line"],
                    },
                },
            },
        ],
    }


def main(args: Namespace):
    codespell = args.codespell
    codespellArgs = args.codespell_args.split(" ")
    codePath = args.path
    uriBaseId = args.base_uri
    outputPath = args.output

    sarif = sarif_template(codePath, uriBaseId, codespell)

    errors = run_codespell(codespell, codespellArgs, codePath)

    for error in errors:
        sarif["runs"][0]["results"].append(parse_error(error, uriBaseId))

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
        "-c", "--codespell",
        help="codespell command to run", type=str, default="codespell"
    )
    argParser.add_argument(
        "-a", "--codespell-args",
        help="codespell arguments", type=str, default="-q 3"
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
