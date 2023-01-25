#!/bin/bash

# delay between each call
DELAY=20

# delay for errors or in moments outside any scheduled operation
DELAY_MAINLOOP=60

# central station email server uucp address
EMAIL_SERVER="gw"

#LATEST_SERVER_CALL_TIME=$(date -u +%s)
#TIME_TO_RUN=$(( ${LATEST_SERVER_CALL_TIME} + ${DELAY_MAINLOOP} ))

while true
do
    timers_start=($(curl -s https://localhost/api/caller -k | jq --raw-output '.[] | select( .enable | contains(1)) | .starttime ' 2> /dev/null))
    timers_stop=($(curl -s https://localhost/api/caller -k | jq --raw-output '.[] | select( .enable | contains(1)) | .stoptime ' 2> /dev/null))

    #    TIME_NOW=$(date -u +%s)
    #    if [ "${TIME_NOW}" -gt "${TIME_TO_RUN}" ]
    #    then
    #      uucico -D -S ${EMAIL_SERVER}
    #      LATEST_SERVER_CALL_TIME=$(date -u +%s)
    #      TIME_TO_RUN=$(( ${LATEST_SERVER_CALL_TIME} + ${DELAY_MAINLOOP} ))
    #    fi

    if [[ -z ${timers_start} ]] || [[ -z ${timers_stop} ]]
    then
      echo "No schedule enabled or API call failed"
      sleep ${DELAY_MAINLOOP}
      continue
    fi

    run_at_least_once=0

    for (( c=0; c<${#timers_start[@]}; c++ )); do

	      tmp="curl -s https://localhost/api/caller/ -k | jq --raw-output '.[${c}] | .stations[]' 2> /dev/null"
	      hosts=$(bash -c "${tmp}")

	      start_time_hour=$((10#$(echo ${timers_start[c]} | cut -d ':' -f 1)))
	      start_time_minute=$((10#$(echo ${timers_start[c]} | cut -d ':' -f 2)))
	      end_time_hour=$((10#$(echo ${timers_stop[c]} | cut -d ':' -f 1)))
	      end_time_minute=$((10#$(echo ${timers_stop[c]} | cut -d ':' -f 2)))

	      current_hour=$((10#$(date +%H)))
	      current_minute=$((10#$(date +%M)))

	      echo "Schedule ${c}, start_time ${start_time_hour}:${start_time_minute}, end_time ${end_time_hour}:${end_time_minute}"
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
                echo "Calling gateway ${EMAIL_SERVER}."
                uucico -D -S ${EMAIL_SERVER}
                sleep ${DELAY}

                # mode/frequency setting stuff
                api_freqmode_call=$(curl -s https://localhost/api/frequency/alias/${t} -k)
                freqmode_enabled=$(echo ${api_freqmode_call} | jq --raw-output '.enable' 2> /dev/null)
                if [[ ${freqmode_enabled} -eq 1  ]]; then
                  old_frequency=$(ubitx_client -c get_frequency 2> /dev/null)
                  old_mode=$(ubitx_client -c get_mode 2> /dev/null)
                  new_frequency="$(echo ${api_freqmode_call} | jq --raw-output '.frequency' 2> /dev/null)000"
                  new_mode=$(echo ${api_freqmode_call} | jq --raw-output '.mode' 2> /dev/null)
                  ubitx_client -c set_frequency -a ${new_frequency}
                  ubitx_client -c set_mode -a ${new_mode}
                fi

                echo "Calling station ${t}."
                uucico -D -S ${t}
                sleep ${DELAY}

                if [[ ${freqmode_enabled} -eq 1  ]]; then
                  ubitx_client -c set_frequency -a ${old_frequency}
                  ubitx_client -c set_mode -a ${old_mode}
                fi

            done
            echo "Calling gateway ${EMAIL_SERVER}."
            uucico -D -S ${EMAIL_SERVER}
        else
            echo "Schedule ${c} will not run now."
        fi

    done

    if [[ ${run_at_least_once} -eq 0 ]]
    then
      sleep ${DELAY_MAINLOOP}
    fi

done
