#!/bin/bash

# Extra options for the calls to the HF stations.  Set to "-Y" to use the
# pre-agreed startup, which replaces the UUCP handshake with a single Y
# command exchange (six line turnarounds down to two).  Needs uucp 1.07-36
# or later, and the gateway must accept it first: a station calling with
# -Y cannot talk to one that does not support it.
#
# This is deliberately not applied to the ${EMAIL_SERVER} calls below,
# which go over the TCP uplink (port TCP, protocol i, with a login chat).
# There is nothing to gain there: the saving is in line turnarounds, which
# a full duplex link does not pay for.
UUCICO_HF_OPTS=""

# delay between each call
DELAY=20

# delay for errors or in moments outside any scheduled operation
DELAY_MAINLOOP=60

# central station email server uucp address
EMAIL_SERVER="gw"

# email server delay between calls
EMAIL_SERVER_DELAY=315
TIME_TO_RUN=$(date -u +%s)
LATEST_SERVER_CALL_TIME=0
#TIME_TO_RUN=$(( ${LATEST_SERVER_CALL_TIME} + ${DELAY_MAINLOOP} ))

while true
do
    # just enabled timers
    timers_start=($(curl -s https://localhost/api/caller -k | jq --raw-output '.[] | select( .enable | contains(1)) | .starttime ' 2> /dev/null))
    timers_stop=($(curl -s https://localhost/api/caller -k | jq --raw-output '.[] | select( .enable | contains(1)) | .stoptime ' 2> /dev/null))

    TIME_NOW=$(date -u +%s)
    if [ ${TIME_NOW} -gt ${TIME_TO_RUN} ]
    then
      uucico -S ${EMAIL_SERVER}
      LATEST_SERVER_CALL_TIME=$(date -u +%s)
      TIME_TO_RUN=$(( ${LATEST_SERVER_CALL_TIME} + ${EMAIL_SERVER_DELAY} ))
    fi

    if [[ -z ${timers_start} ]] || [[ -z ${timers_stop} ]]
    then
      echo "No schedule enabled or API call failed"
      sleep ${DELAY_MAINLOOP}
      continue
    fi

    # all timers
    timers_start=($(curl -s https://localhost/api/caller -k | jq --raw-output '.[] | .starttime ' 2> /dev/null))
    timers_stop=($(curl -s https://localhost/api/caller -k | jq --raw-output '.[] | .stoptime ' 2> /dev/null))

    run_at_least_once=0

    for (( c=0; c<${#timers_start[@]}; c++ )); do
	      tmp="curl -s https://localhost/api/caller -k | jq --raw-output '.[${c}] | select( .enable | contains(1)) | .starttime' 2> /dev/null"
	      is_enabled=$(bash -c "${tmp}")

	      if [[ -z ${is_enabled} ]]
	      then
              continue
	      fi

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
	      if [[ ${current_hour}   -eq ${start_time_hour} &&
                ${current_hour}   -eq ${end_time_hour} &&
                ${current_minute} -ge ${start_time_minute} &&
                ${current_minute} -le ${end_time_minute} ]] ||
             [[ ${current_hour}   -eq ${start_time_hour} &&
                ${current_hour}   -lt ${end_time_hour} &&
                ${current_minute} -ge ${start_time_minute} ]] ||
             [[ ${current_hour}   -gt ${start_time_hour} &&
                ${current_hour}   -eq ${end_time_hour} &&
                ${current_minute} -le ${end_time_minute} ]] ||
             [[ ${current_hour}   -gt ${start_time_hour} &&
		        ${current_hour}   -lt ${end_time_hour} ]]
        then
            run_at_least_once=1
            for t in ${hosts[*]}; do
                echo "Calling gateway ${EMAIL_SERVER}."
                uucico -D -S ${EMAIL_SERVER}
                # sleep ${DELAY}

                # mode/frequency setting stuff
                api_freqmode_call=$(curl -s https://localhost/api/frequency/alias/${t} -k)
                freqmode_enabled=$(echo ${api_freqmode_call} | jq --raw-output '.enable' 2> /dev/null)
                if [[ ${freqmode_enabled} -eq 1  ]]; then
                  old_frequency=$(sbitx_client -c get_frequency 2> /dev/null)
                  old_mode=$(sbitx_client -c get_mode 2> /dev/null)
                  new_frequency="$(echo ${api_freqmode_call} | jq --raw-output '.frequency' 2> /dev/null)000"
                  new_mode=$(echo ${api_freqmode_call} | jq --raw-output '.mode' 2> /dev/null)
                  sbitx_client -c set_frequency -a ${new_frequency} > /dev/null
                  sbitx_client -c set_mode -a ${new_mode} > /dev/null
                fi

                echo "Calling station ${t}."
                uucico ${UUCICO_HF_OPTS} -m -D -S ${t}
                sleep ${DELAY}

                if [[ ${freqmode_enabled} -eq 1  ]]; then
                  sbitx_client -c set_frequency -a ${old_frequency} > /dev/null
                  sbitx_client -c set_mode -a ${old_mode} > /dev/null
                fi

            done
            echo "Calling gateway ${EMAIL_SERVER}."
            uucico -S ${EMAIL_SERVER}
        else
            echo "Schedule ${c} will not run now."
        fi

    done

    if [[ ${run_at_least_once} -eq 0 ]]
    then
      sleep ${DELAY_MAINLOOP}
    fi

done
