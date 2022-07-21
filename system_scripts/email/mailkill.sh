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
  lang="es"
  type=${1}
  uuid=${2}
fi

#get uucp host path prefix
host=$(echo $uuid | cut -d "." -f1)

#get uuidwh:  uuid without host
uuidwh=$(echo $uuid | cut -d "." -f2)

fullC=/var/spool/uucp/$host/C./C.$uuidwh
if [ ! -f $fullC ] ;  then
  echo "error - no C file... exiting"
  exit
fi

#filter crmail from C and get D
D=$(cat $fullC | grep crmail | awk '{print $2 ;}')

#get destination
to=$(cat $fullC | cut -d ' ' -f 11- )
echo "To: " $to

fullD="/var/spool/uucp/$host/D./$D"
if [ ! -f $fullD ] ; then
  echo "error - no D file... exiting"
  exit
fi
echo "FullD " = $fullD

from=$(zcat $fullD | head -n1 | awk '{print $2;}')

subject=$(zcat $fullD | head -n20 |grep Subject| awk '{print $2;}')

local_time="$(date -d @$(stat -c %W ${fullC}) '+%H:%M %d/%m/%Y' )"

kill=$(uustat -k $uuid)

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

#send email
echo -e ${message} | mail  -s "${subject}" ${from}
