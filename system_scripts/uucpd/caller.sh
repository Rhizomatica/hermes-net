#!/bin/bash

# delay between each call
DELAY=2

# delay for errors or in moments outside any scheduled operation
DELAY_MAINLOOP=15

# central station email server uucp address
EMAIL_SERVER="gw"

LATEST_SERVER_CALL_TIME=$(date -u +%s)
TIME_TO_RUN=$(( ${LATEST_SERVER_CALL_TIME} + ${DELAY_MAINLOOP} ))

while true
do
    hosts=($(curl -s http://localhost/api/caller/ | jq --raw-output '.[0] | .stations[]'  2> /dev/null)) 
    timers_start=($(curl -s http://localhost/api/caller | jq --raw-output '.[] | select( .enable | contains(1)) | .starttime ' 2> /dev/null))
    timers_stop=($(curl -s http://localhost/api/caller | jq --raw-output '.[] | select( .enable | contains(1)) | .stoptime ' 2> /dev/null))

    TIME_NOW=$(date -u +%s)
    if [ "${TIME_NOW}" -gt "${TIME_TO_RUN}" ]
    then
      uucico -D -S ${EMAIL_SERVER}
      LATEST_SERVER_CALL_TIME=$(date -u +%s)
      TIME_TO_RUN=$(( ${LATEST_SERVER_CALL_TIME} + ${DELAY_MAINLOOP} ))
    fi

    if [[ -z ${hosts} ]] || [[ -z ${timers_start} ]] || [[ -z ${timers_stop} ]]
    then
      echo "No schedule enabled or API call failed"
      sleep ${DELAY_MAINLOOP}
      continue
    fi

    run_at_least_once=0

    for (( c=0; c<${#timers_start[@]}; c++ )); do

	      start_time_hour=$((10#$(echo ${timers_start[c]} | cut -d ':' -f 1)))
	      start_time_minute=$((10#$(echo ${timers_start[c]} | cut -d ':' -f 2)))
	      end_time_hour=$((10#$(echo ${timers_stop[c]} | cut -d ':' -f 1)))
	      end_time_minute=$((10#$(echo ${timers_stop[c]} | cut -d ':' -f 2)))

	      current_hour=$((10#$(date +%H)))
	      current_minute=$((10#$(date +%M)))

	      echo "Schedule  ${c}"
#	      echo "current time ${current_hour}h ${current_minute}min"
#	      echo "start time ${start_time_hour}h ${start_time_minute}min"
#	      echo "end time ${end_time_hour}h ${end_time_minute}min"

	      if [[ ${current_hour} -eq ${start_time_hour} &&
		              ${current_minute} -ge ${start_time_minute} ]] ||
               [[ ${current_hour} -gt ${start_time_hour} &&
		                  ${current_hour} -lt ${end_time_hour} ]] ||
               [[ ${current_hour} -eq ${end_time_hour} &&
                      ${current_minute} -le ${end_time_minute} ]]

	      then

                  run_at_least_once=1
	          for t in ${hosts[*]}; do
		    echo "Calling ${t}"
		    uucico -D -S ${t}
		    sleep ${DELAY}
		    # here we sync with server again... as connection times can be veeery long
		    uucico -D -S ${EMAIL_SERVER}
	          done

	      else
	          echo "Schedule ${c} will not run now."
	      fi

    done

    if [[ ${run_at_least_once} -eq 0 ]]
    then
      sleep ${DELAY_MAINLOOP}
    fi

done
