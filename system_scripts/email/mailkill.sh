#!/bin/bash

if [ $# -eq 0 ] || [ $# -eq 1 ] || [ $# -gt 3 ]; then
  echo "Usage: $0 [es|pt|en] size_limit|queue_full|gui uuid"
  exit 1
fi

#get args
if [ $# -eq 3 ]; then
  lang=${1}
  type=${2}
  uuid=${3}
elif [ $# -eq 2 ]; then
  lang=$(curl -s "https://localhost/api/sys/language" -k 2> /dev/null)
  type=${1}
  uuid=${2}
fi

#get host path prefix
host=$(echo $uuid | cut -d "." -f1)

#get uuidwh:  uuid without host
uuidwh=$(echo $uuid | cut -d "." -f2)

NNCP_SPOOL=${NNCP_SPOOL:=/var/spool/nncp}
NNCP_JOBS=${NNCP_SPOOL}/hermes-jobs

# NNCP or UUCP is decided per mail, not per station: a station that ran
# NNCP and went back to UUCP keeps /etc/nncp.hjson, and its UUCP mail must
# still be cancelled. A mail uuxcomp queued over NNCP has a job index entry
# (uuid is <node>.<packet id>); anything else is a UUCP job.
job=${NNCP_JOBS}/${uuidwh}
if [ -f "${job}" ]; then
  # NNCP: the packet is encrypted, so the details come from the job index

  to=$(grep '^To: ' $job | cut -d ' ' -f 2-)
  from=$(grep '^From: ' $job | cut -d ' ' -f 2-)
  subject=$(grep '^Subject: ' $job | cut -d ' ' -f 2-)
  echo "To: " $to

  when=$(grep '^When: ' $job | cut -d ' ' -f 2-)
  local_time="$(date -d "${when}" '+%H:%M %d/%m/%Y' 2>/dev/null)"
  if [ -z "${local_time}" ]; then
    local_time="$(date -d @$(stat -c %W ${job}) '+%H:%M %d/%m/%Y' )"
  fi

  kill=$(nncp-rm -node $host -pkt $uuidwh)
  rm -f $job
else
  fullC=/var/spool/uucp/$host/C./C.$uuidwh
  if [ ! -f $fullC ] ;  then
    echo "error - no C file... exiting"
    exit 1
  fi

  #filter crmail from C and get D
  D=$(cat $fullC | grep crmail | awk '{print $2 ;}')

  #get destination
  to=$(cat $fullC | cut -d ' ' -f 11- )
  echo "To: " $to

  fullD="/var/spool/uucp/$host/D./$D"
  if [ ! -f $fullD ] ; then
    echo "error - no D file... exiting"
    exit 1
  fi
  echo "FullD " = $fullD

  # uuxcomp compresses with xz; older queued mail may be gzip, or plain
  unpack() { if xz -t "$1" 2> /dev/null; then xzcat "$1"; else zcat -f "$1"; fi; }

  from=$(unpack $fullD | head -n1 | awk '{print $2;}')

  subject=$(unpack $fullD | head -n20 |grep Subject| awk '{print $2;}')

  local_time="$(date -d @$(stat -c %W ${fullC}) '+%H:%M %d/%m/%Y' )"

  kill=$(uustat -k $uuid)
fi

if [ $lang = "en" ] && [ $type = "gui" ]; then
  message="Your email with destination(s): ${to} sent at ${local_time} was canceled by the admin user. \n\nThis is an automatic message from HERMES System!"
  subject="Email canceled by the admin user"
elif [ $lang = "en" ] && [ $type = "size_limit" ]; then
  message="Your email with destination(s): ${to} sent at ${local_time} was canceled because exceeds maximum email size limit. \n\nThis is an automatic message from HERMES System!"
  subject="Email canceled by the system"
elif [ $lang = "en" ] && [ $type = "queue_full" ]; then
  message="Your email with destination(s): ${to} sent at ${local_time} was canceled because transmission list exceeds maximum size. \n\nThis is an automatic message from HERMES System!"
  subject="Email canceled by the system"

elif [ $lang = "es" ] && [ $type = "gui" ]; then
  message="Su correo electrónico con destino(s): ${to} enviado el ${local_time} fue cancelado por el usuario administrador. \n\nEste es un mensaje del sistema automático de HERMES!"
  subject="Email cancelado por el administrador"
elif [ $lang = "es" ] && [ $type = "size_limit" ]; then
  message="Su correo electrónico con destino(s): ${to} enviado el ${local_time} fue cancelado porque supera el límite máximo de tamaño de correo electrónico. \n\nEste es un mensaje del sistema automático de HERMES!"
  subject="Email cancelado por el sistema"
elif [ $lang = "es" ] && [ $type = "queue_full" ]; then
  message="Su correo electrónico con destino(s): ${to} enviado el ${local_time} fue cancelado porque la lista de transmisión excede el tamaño máximo. \n\nEste es un mensaje del sistema automático de HERMES!"
  subject="Email cancelado por el sistema"
fi

if [ $lang = "pt" ] && [ $type = "gui" ]; then
  message="Seu e-mail com destino(s): ${to} enviado em ${local_time} foi cancelado pelo usuário administrador. \n\nEsta é uma mensagem automática do Sistema HERMES!"
  subject="Email cancelado pelo usuário administrador"
elif [ $lang = "pt" ] && [ $type = "size_limit" ]; then
  message="Seu e-mail com destino(s): ${to} enviado em ${local_time} foi cancelado porque excede o limite máximo de tamanho de e-mail. \n\nEsta é uma mensagem automática do Sistema HERMES!"
  subject="E-mail cancelado pelo sistema"
elif [ $lang = "pt" ] && [ $type = "queue_full" ]; then
  message="Seu e-mail com destino(s): ${to} enviado em ${local_time} foi cancelado porque a lista de transmissão excede o tamanho máximo. \n\nEsta é uma mensagem automática do Sistema HERMES!"
  subject="E-mail cancelado pelo sistema"
fi

if [ $lang = "fr" ] && [ $type = "gui" ]; then
  message="Votre email avec destination(s) : ${to} envoyé à ${local_time} a été annulé par \n\nl'administrateur.Ceci est un message automatique du système HERMES !"
  subject="Email annulé par l'utilisateur administrateur"
elif [ $lang = "fr" ] && [ $type = "size_limit" ]; then
  message="Votre email avec destination(s) : ${to} envoyé à ${local_time} a été annulé parce qu'il dépasse la taille limite par email. Ceci est un message automatique du système HERMES !"
  subject="Email annulé par le système"
elif [ $lang = "fr" ] && [ $type = "queue_full" ]; then
  message="Votre email avec destination(s) : ${to} envoyé à ${local_time} a été annulé parce que la liste de transmission dépasse la taille limite. Ceci est un message automatique du système HERMES !"
  subject="Email annulé par le système"
fi

#send email
echo -e ${message} | mail -a "Chat-Version: 1.0" -s "${subject}" ${from}
