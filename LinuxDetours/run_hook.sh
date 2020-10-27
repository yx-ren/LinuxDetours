#/bin/bash

output_dir="./detected_results"
rm -rf $output_dir
mkdir $output_dir

exec_files=(\
    "/usr/bin/wechat"\
    "/usr/local/bin/emacs"\
    "/usr/share/tencent-qq/qq"\
    "/opt/FoxitSoftware/FoxitOfficeSuite/FoxitOfficeSuite"\
    "/opt/kingsoft/wps-office/office6/wps"\
    "/opt/kingsoft/wps-office/office6/wpp"\
    "/opt/kingsoft/wps-office/office6/et" \
    )

for ((i = 0; i != ${#exec_files[@]}; i++))
do
    exec_file=${exec_files[i]}
    filename=$(echo $exec_file | awk -F '/' {'print $NF'})
    detected_result="$filename.detected"
    echo "-------------------- detect begin --------------------"
    echo "detect file path:[$exec_file]"
    ./hook.sh $exec_file > "$output_dir/$detected_result" 2>&1
    echo "detect file:[$filename] finish, result saved in:[$output_dir/$detected_result]"
    echo "-------------------- detected done --------------------"
    echo -e "\n"
done

