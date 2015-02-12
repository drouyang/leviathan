
IFS="
" # no, you can't actually use "\n" to specify a newline....

pushd ../ > /dev/null
for cfg in `cat ./hobbes.cfg`; do
    if  [ ! -z ${cfg} ]; then
        eval export ${cfg}
    fi
done
popd > /dev/null
IFS=" "
