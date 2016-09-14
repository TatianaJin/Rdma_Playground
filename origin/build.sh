fn=$1
bfn=$(basename $1)
output=${bfn%.*}

HUSKY_ROOT=/data/yuzhen/play/play/tmp/h3/husky/
echo "Building... (using HUSKY_ROOT=${HUSKY_ROOT})"

gcc  \
    $1 \
    -I${HUSKY_ROOT} \
    -libverbs -lzmq \
    -o $output \
    -g
echo "Build finished"

