#!/bin/bash
PORT=8300
CMD="nc -z localhost ${PORT}"

${CMD}
retval=$?
while [ "${retval}" != "0" ]
do
    sleep 1
    ${CMD}
    retval=$?
done
