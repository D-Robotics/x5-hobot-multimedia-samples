export BPLAT_BUSY_THRES=1
SCRIPT_DIR="$( cd "$( dirname "$(readlink -f "${BASH_SOURCE[0]}")" )" && pwd )"
PORTION=100
TH_VALUE1=80   #for two bpu core to correct portion value
TH_VALUE2=50   #for single bpu core to correct portion value1
TH_VALUE3=80   #for single bpu core to correct portion value2
FULL=100
COR1=2
COR2=5
BPU=0
usage="Usage: $0 [-b bpu core] [-p portion]"

while getopts "p:b:h" opt
do
    case $opt in
    p)
        PORTION=$OPTARG
        ;;
    b)        
        BPU=$OPTARG
        ;;               
    h)        
        echo $usage
        exit 0     
        ;;    
    \?)       
        echo $usage
        exit 1     
        ;;    
    esac      
done        
echo "bpu:$BPU"
if [ $BPU -eq 1 ] || [ $BPU -eq 0 ]; then
        if [ $PORTION -lt $TH_VALUE2 ]; then
                echo "portion:$PORTION,execute I0001"
                PORTION=`expr $PORTION - $COR1`
                $SCRIPT_DIR/../models/HBDK3_MODEL_I0001/run.sh $PORTION $BPU
	elif [ $PORTION -ge $TH_VALUE2 ] && [ $PORTION -lt $TH_VALUE3 ]; then
                echo "portion:$PORTION,execute I0012"
                PORTION=`expr $PORTION - $COR2`
                $SCRIPT_DIR/../models/HBDK3_MODEL_I0012/run.sh $PORTION $BPU
        elif [ $PORTION -eq $FULL ]; then
                echo "portion:$PORTION,execute 2K"
                $SCRIPT_DIR/../models/HBDK3_MODEL_2K/run.sh $PORTION $BPU
        else
                echo "portion:$PORTION,execute P0000"
                PORTION=`expr $PORTION - $COR2`
                $SCRIPT_DIR/../models/HBDK3_MODEL_P0000/run.sh $PORTION $BPU
        fi
elif [ $BPU -eq 2 ]; then
        if [ $PORTION -lt $TH_VALUE1 ]; then
                echo "portion:$PORTION,execute HBDK3_MODEL_P0000"
                PORTION=`expr $PORTION - $COR1`
                $SCRIPT_DIR/../models/HBDK3_MODEL_P0000/run.sh $PORTION $BPU
        elif [ $PORTION -eq $FULL ]; then
                echo "portion:$PORTION,execute HBDK3_MODEL_2K"
                $SCRIPT_DIR/../models/HBDK3_MODEL_2K/run.sh $PORTION $BPU
        else
        	echo "portion is $PORTION"
                PORTION=`expr $PORTION - $COR2`
                $SCRIPT_DIR/../models/HBDK3_MODEL_P0000/run.sh $PORTION $BPU
        fi
else
        echo "select bpu parameter 0/1/2"
fi

