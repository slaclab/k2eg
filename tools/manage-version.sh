commandoutput="$(gitversion)"
MAJOR_VERSION="$(echo "$commandoutput" | jq '.Major')"
MINOR_VERSION="$(echo "$commandoutput" | jq '.Minor')"
PATCH_VERSION="$(echo "$commandoutput" | jq '.Patch')"
echo "#define k2eg_VERSION \"v$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION\"" > $1