#/bin/bash
CMAKE_FILE="CMakeLists.txt"

CURRENT_MAJOR_VERSION=$(grep "DETOURS_MAJOR_VERSION" $CMAKE_FILE | head -1 | awk '{print $2}' | cut -d ')' -f 1)
CURRENT_MINOR_VERSION=$(grep "DETOURS_MINOR_VERSION" $CMAKE_FILE | head -1 | awk '{print $2}' | cut -d ')' -f 1)
CURRENT_PATCH_VERSION=$(grep "DETOURS_PATCH_VERSION" $CMAKE_FILE | head -1 | awk '{print $2}' | cut -d ')' -f 1)

CURRENT_BUILD_NO=$(grep "DETOURS_BUILD_NO" $CMAKE_FILE | head -1 | awk '{print $2}' | cut -d ')' -f 1)
NEXT_BUILD_NO=$(echo $CURRENT_BUILD_NO | awk '{printf("%03d\n", $1 + 1);}')

CURRENT_BUILD_TAG="DETOURS_BUILD_NO $CURRENT_BUILD_NO"
NEXT_BUILD_TAG="DETOURS_BUILD_NO $NEXT_BUILD_NO"

sed -i "s/$CURRENT_BUILD_TAG/$NEXT_BUILD_TAG/" $CMAKE_FILE
VERSION_PREFIX=$CURRENT_MAJOR_VERSION.$CURRENT_MINOR_VERSION.$CURRENT_PATCH_VERSION
GIT_COMMIT_COMMENT="modify project version from '$VERSION_PREFIX.$CURRENT_BUILD_NO' to '$VERSION_PREFIX.$NEXT_BUILD_NO'"
echo $GIT_COMMIT_COMMENT
git add -u
git commit -m "$GIT_COMMIT_COMMENT"
git tag $VERSION_PREFIX.$NEXT_BUILD_NO
