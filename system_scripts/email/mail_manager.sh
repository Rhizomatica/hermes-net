#!/bin/bash
# Usage mail_manager.sh [uucp C. file]


# maximum email size
MAX_EMAIL_SIZE=${MAX_EMAIL_SIZE:=20000}

# maximum queue size
MAX_UUCP_QUEUE=${MAX_UUCP_QUEUE:=80000}

# turn on log on syslog, disabled by default
SYSLOG=false

function syslog(){
  if $SYSLOG; then
      echo $@ | systemd-cat -t mail_manager -p info
    else
      echo $@
  fi
}

# input paramenters: $HOST
get_uucp_queue_size_for_host ()
{
  size=$(uustat -s ${HOST} | grep crmail | egrep -o '(\w+)\sbytes' | awk -F ' ' '{sum+=$1; } END {print sum}' 2> /dev/null)
  total_size=$(printf '%d' ${size} 2>/dev/null)

## old code without using uustat
#  total_size=0
#  if [ -d /var/spool/uucp/${HOST}/C. ]
#  then
#    echo "Calculating size of email uucp queue for host: ${HOST}."
#    cd /var/spool/uucp/${HOST}/C.
#    for k in $(ls -1)
#    do
#      cmd=$(cat ${k} | cut -d ' '  -f 10 )
#      if [ "${cmd}" == "crmail" ]
#     then
#         D=$(cat ${k} | grep crmail | awk '{print $2 ;}')
#         D_size=$(stat -c %s ../D./${D})
#         if [[ $D_size =~ ^[+-]?[0-9]+$ ]]
#         then
#           let total_size=total_size+D_size
#         fi
#      fi
#    done
#  fi
}

# Uucp C. file passed in argv[1]
if [ ! -z "${1}" ]; then

  host=$(echo ${1}  | rev | cut -d '/' -f 3 | rev)
  C=$(echo ${1}  | rev | cut -d '/' -f 1 | rev)
  syslog "Looking at system ${host}"

  # enforce the email being added conforms to the maximum size
  if [ -d "/var/spool/uucp/${host}/C." ]
  then
    cd /var/spool/uucp/${host}/C.

    cmd=$(cat ${C} | cut -d ' '  -f 10 )

    if [ "${cmd}" == "crmail" ]
    then
      D=$(cat ${C} | grep crmail | awk '{print $2 ;}')
      D_size=$(stat -c %s ../D./${D})
      id=$(echo ${C} | cut -d . -f 2)
      uuid="${host}.${id}"
      if [ ${D_size} -gt ${MAX_EMAIL_SIZE} ]
      then
        syslog "UUID = ${uuid} SIZE ${D_size} WILL ME MAILKILLED!"
        mailkill.sh size_limit ${uuid}
      else
        syslog "UUID = ${uuid} SIZE ${uuid} IS GOOD"
      fi
    fi
  fi

  # now check the size of the queue...
  total_size=0
  if [ -d "/var/spool/uucp/${host}/C." ]
  then
    HOST=${host}
    get_uucp_queue_size_for_host

    cd /var/spool/uucp/${host}/C.

    while [ "${total_size}" -gt "${MAX_UUCP_QUEUE}" ]
    do
      newest_timestamp="0"
      syslog "Total size of the UUCP (${total_size}) exceed the maximum of ${MAX_UUCP_QUEUE}. Deleting last email."

      for j in $(ls -1)
      do
        # check if it is mail
        cmd=$(cat ${j} | cut -d ' '  -f 10 )
        if [ "${cmd}" == "crmail" ]
        then
          # get the stat in unix seconds timestamp pra ficar facil de achar quem eh o mais novo...
          time_stamp="$( stat -c '%W' ${j} )"
          if [ "${time_stamp}" -gt "${newest_timestamp}" ]
          then
            newest_timestamp="${time_stamp}"
            id=$(echo ${j} | cut -d . -f 2)
            uuid="${host}.${id}"
          fi
        fi
      done

      syslog "Deleting uuid: ${uuid} ${newest_timestamp}"
      # delete the newest email...
      mailkill.sh queue_full ${uuid}

      get_uucp_queue_size_for_host
    done
    syslog "Total UUCP email queue size ${total_size} to ${HOST} is good"
  fi

  ## don't use this mode anymore... kept for compatibility purposes (no argv[1])
else

  # check individual email sizes
  for i in $(ls -1 /var/spool/uucp/)
  do
    if [ -d /var/spool/uucp/${i}/C. ]
    then
      syslog "Looking at system ${i}"
      cd /var/spool/uucp/${i}/C.
      for j in $(ls -1)
      do

        # check if it is mail
        cmd=$(cat ${j} | cut -d ' '  -f 10 )
        if [ "${cmd}" == "crmail" ]
        then
          D=$(cat ${j} | grep crmail | awk '{print $2 ;}')
          D_size=$(stat -c %s ../D./${D})
          id=$(echo ${j} | cut -d . -f 2)
          uuid="${i}.${id}"
          if [ ${D_size} -gt ${MAX_EMAIL_SIZE} ]
          then
            syslog "UUID = ${uuid} SIZE ${D_size} WILL ME MAILKILLED!"
            mailkill.sh size_limit ${uuid}
          else
            syslog "UUID = ${uuid} SIZE ${uuid} IS GOOD"
          fi
        fi
      done
    fi
  done

  # now check for the size of the emails in the uucp queue for each host...
  # and delete the newest emails.
  total_size=0
  for i in $(ls -1 /var/spool/uucp/)
  do
    if [ -d /var/spool/uucp/${i}/C. ]
    then
      HOST=${i}
      get_uucp_queue_size_for_host
      syslog "UUCP email queue for ${HOST} is ${total_size} bytes."

      # total_size=$(uustat -a | grep -v ' rmail ' |  awk -F ' ' '{sum+=$(NF - 1);}END{print sum;}')

      cd /var/spool/uucp/${HOST}/C.

      while [ "${total_size}" -gt "${MAX_UUCP_QUEUE}" ]
      do
        newest_timestamp="0"
        syslog "Total size of the UUCP (${total_size}) exceed the maximum of ${MAX_UUCP_QUEUE}. Deleting last email."

        for j in $(ls -1)
        do
          # check if it is mail
          cmd=$(cat ${j} | cut -d ' '  -f 10 )
          if [ "${cmd}" == "crmail" ]
          then
            # get the stat in unix seconds timestamp pra ficar facil de achar quem eh o mais novo...
            time_stamp="$( stat -c '%W' ${j} )"
            if [ "${time_stamp}" -gt "${newest_timestamp}" ]
            then
              newest_timestamp="${time_stamp}"
              id=$(echo ${j} | cut -d . -f 2)
              uuid="${i}.${id}"
            fi
          fi
        done

        syslog "Deleting uuid: ${uuid} ${newest_timestamp}"
        # delete the newest email...
        mailkill.sh queue_full ${uuid}

        get_uucp_queue_size_for_host
      done
      syslog "Total UUCP email queue size ${total_size} to ${HOST} is good"
    fi

  done

fi
