#!/bin/bash

set -e

if [[ $# -lt 4 ]]; then
    echo "Usage: $0 <version> <commit> <remote> <release zip file>"
    echo "Example: $0 0.1.0 123456 origin release.zip"
    echo "run inside the zcbor repository"
    exit 1
fi

version=$1
commit=$2
remote=$3
release_zip=$4
base=$5

if [[ "$(git show $commit:zcbor/VERSION)" != "$version" ]]; then
    echo "Version in VERSION file does not match the version provided"
    exit 2
fi

if [[ ! "$release_zip" =~ "$version" ]]; then
    echo "Release zip file name should contain the version"
    exit 3
fi

if [[ ! -f $release_zip ]]; then
    echo "Release zip file not found"
    exit 4
fi

if [[ -d zcbor-release-files-$version ]]; then
    echo "Release files directory (zcbor-release-files-$version) already exists"
    exit 5
fi

if ! git merge-base --is-ancestor $remote/$base $commit; then
    echo "Release commit is not based on upstream $base branch"
    exit 6
fi

# Create tag
git tag $version $commit
# Push tag
git push $remote $version
# Push commit to main branch
git push $remote $commit:$base
# Unzip release files
mkdir zcbor-release-files-$version
unzip $release_zip -d zcbor-release-files-$version
# Create release on GitHub
gh release create -d $version zcbor-release-files-$version/* --repo $(git remote get-url $remote) --title "zcbor v. $version" --notes "Release notes: [RELEASE_NOTES.md](https://github.com/nordicsemi/zcbor/blob/$version/RELEASE_NOTES.md)

Migration guide: [MIGRATION_GUIDE.md](https://github.com/nordicsemi/zcbor/blob/$version/MIGRATION_GUIDE.md)

Pypi: https://pypi.org/project/zcbor/$version/"
# Create release on Pypi
twine upload --verbose zcbor-release-files-$version/*
# Clean up
rm -rf zcbor-release-files-$version
