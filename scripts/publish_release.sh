#!/bin/bash
#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

# Tag and publish a zcbor release: push the tag and the release commit, create a draft release on
# GitHub with the release files, and (unless --no-pypi is given) upload them to PyPI.
# Run inside the zcbor repository.

set -e

usage() {
    echo "Usage: $0 [--no-pypi] <version> <commit> <remote> <release zip file> [base branch]"
    echo "  --no-pypi    Don't upload the release files to PyPI."
    echo "  base branch  The branch the release commit is pushed to (default: main),"
    echo "               e.g. a release branch for a backport release."
    echo "Example: $0 0.1.0 123456 origin zcbor-release-0.1.0.zip"
    echo "Example: $0 --no-pypi 0.8.3 123456 origin zcbor-release-0.8.3.zip release/0.8.3"
}

pypi=true
if [[ "$1" == "--no-pypi" ]]; then
    pypi=false
    shift
fi

if [[ $# -lt 4 || $# -gt 5 ]]; then
    usage
    exit 1
fi

version=$1
commit=$2
remote=$3
release_zip=$4
base=${5:-main}
release_dir=zcbor-release-files-$version

if [[ "$(git show "$commit:zcbor/VERSION")" != "$version" ]]; then
    echo "Version in VERSION file does not match the version provided"
    exit 2
fi

if [[ ! "$release_zip" =~ "$version" ]]; then
    echo "Release zip file name should contain the version"
    exit 3
fi

if [[ ! -f "$release_zip" ]]; then
    echo "Release zip file not found"
    exit 4
fi

if [[ -d "$release_dir" ]]; then
    echo "Release files directory ($release_dir) already exists"
    exit 5
fi

if ! git merge-base --is-ancestor "$remote/$base" "$commit"; then
    echo "Release commit is not based on $remote/$base"
    exit 6
fi

if git rev-parse -q --verify "refs/tags/$version" > /dev/null \
        || [[ -n "$(git ls-remote --tags "$remote" "refs/tags/$version")" ]]; then
    echo "Tag $version already exists (locally or in $remote)"
    exit 7
fi

for tool in gh unzip $($pypi && echo twine); do
    if ! command -v "$tool" > /dev/null; then
        echo "$tool not found"
        exit 8
    fi
done

notes="Release notes: [RELEASE_NOTES.md](https://github.com/nordicsemi/zcbor/blob/$version/RELEASE_NOTES.md)

Migration guide: [MIGRATION_GUIDE.md](https://github.com/nordicsemi/zcbor/blob/$version/MIGRATION_GUIDE.md)"
if $pypi; then
    notes="$notes

Pypi: https://pypi.org/project/zcbor/$version/"
fi

mkdir "$release_dir"
# Remove the unzipped release files when done, also if a step fails.
trap 'rm -rf "$release_dir"' EXIT

git tag "$version" "$commit"
git push "$remote" "$version"
git push "$remote" "$commit:$base"
unzip "$release_zip" -d "$release_dir"

gh release create -d "$version" "$release_dir"/* --repo "$(git remote get-url "$remote")" \
    --title "zcbor v. $version" --notes "$notes"

if $pypi; then
    twine upload --verbose "$release_dir"/*
fi
