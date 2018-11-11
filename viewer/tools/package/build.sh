cur=$(cd $(dirname $0) && pwd)
major=`grep -Po 'RAYCOIN_VERSION_MAJOR \K(\d+)' $cur/../../src/RaycoinViewer.h | tr -d '\n'`
minor=`grep -Po 'RAYCOIN_VERSION_MINOR \K(\d+)' $cur/../../src/RaycoinViewer.h | tr -d '\n'`
out="Raycoin-v$major.$minor.zip"
dir="$cur/../../Build/package"
rm -rf $dir
mkdir -p $dir
cp $cur/../../Build/x64/Release/Output/RaycoinViewer/RaycoinViewer.exe $dir
cp -r $cur/inc/* $dir
pushd $dir
zip -r $out .
popd
mv -f $dir/$out .