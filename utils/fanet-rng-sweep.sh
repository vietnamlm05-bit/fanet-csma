#!/usr/bin/env bash
set -euo pipefail

NS3_DIR="${NS3_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
RUNS="${RUNS:-5}"
MIN_N="${MIN_N:-2}"
MAX_N="${MAX_N:-30}"
SIM_TIME="${SIM_TIME:-1800}"
INCLUDE_STATIC="${INCLUDE_STATIC:-1}"
WRITE_XML="${WRITE_XML:-true}"
TAG="${TAG:-$(date +%Y%m%d-%H%M%S)}"

if [ "$MIN_N" -lt 2 ] || [ "$MAX_N" -gt 30 ] || [ "$MIN_N" -gt "$MAX_N" ]; then
  echo "MIN_N/MAX_N must stay within 2..30." >&2
  exit 1
fi

if [ "$RUNS" -lt 1 ]; then
  echo "RUNS must be >= 1." >&2
  exit 1
fi

cd "$NS3_DIR"
mkdir -p results/csv results/logs

modes=("mobile")
if [ "$INCLUDE_STATIC" = "1" ] || [ "$INCLUDE_STATIC" = "true" ]; then
  modes+=("static")
fi

rawSummary="results/csv/fanet-rng-${MIN_N}to${MAX_N}-${SIM_TIME}s-raw-summary-${TAG}.csv"
rawMilestones="results/csv/fanet-rng-${MIN_N}to${MAX_N}-${SIM_TIME}s-raw-milestones-${TAG}.csv"
aggSummary="results/csv/fanet-rng-${MIN_N}to${MAX_N}-${SIM_TIME}s-aggregate-summary-${TAG}.csv"
aggMilestones="results/csv/fanet-rng-${MIN_N}to${MAX_N}-${SIM_TIME}s-aggregate-milestones-${TAG}.csv"
logFile="results/logs/fanet-rng-${MIN_N}to${MAX_N}-${SIM_TIME}s-${TAG}.log"
xmlDir="results/flowmon/rng-${MIN_N}to${MAX_N}-${SIM_TIME}s-${TAG}"

: > "$rawSummary"
: > "$rawMilestones"
: > "$logFile"
mkdir -p "$xmlDir"

for run in $(seq 1 "$RUNS"); do
  for mode in "${modes[@]}"; do
    mobileFlag="true"
    if [ "$mode" = "static" ]; then
      mobileFlag="false"
    fi

    for n in $(seq "$MIN_N" "$MAX_N"); do
      xmlFile="${xmlDir}/fanet-csma-n${n}-${mode}-rng${run}-${SIM_TIME}s.flowmon.xml"
      echo "RUN mode=${mode} nNodes=${n} rngRun=${run} simTime=${SIM_TIME}"
      ./ns3 run "fanet-csma-simulation --mobile=${mobileFlag} --nNodes=${n} --rngRun=${run} --simTime=${SIM_TIME} --csvFile=${rawSummary} --milestoneCsvFile=${rawMilestones} --flowXmlFile=${xmlFile} --writeFlowXml=${WRITE_XML}" >> "$logFile" 2>&1
    done
  done
done

awk -F, -v OFS=',' '
function add(key, name, value) {
  sum[key, name] += value
  sumsq[key, name] += value * value
}
function avg(key, name) {
  return sum[key, name] / count[key]
}
function stddev(key, name, n, mean, variance) {
  n = count[key]
  if (n < 2) {
    return 0
  }
  mean = sum[key, name] / n
  variance = (sumsq[key, name] - n * mean * mean) / (n - 1)
  if (variance < 0 && variance > -0.000000001) {
    variance = 0
  }
  return sqrt(variance)
}
NR == 1 {
  next
}
{
  key = $1 SUBSEP $2
  if (!(key in seen)) {
    seen[key] = 1
    keys[++keyCount] = key
    mode[key] = $1
    nodes[key] = $2
    clients[key] = $3
    rts[key] = $4
    packetSize[key] = $5
    packetRate[key] = $6
    simTime[key] = $7
    serverStart[key] = $8
    clientStart[key] = $9
    offered[key] = $10
  }
  count[key]++
  add(key, "pdr", $17)
  add(key, "loss", $18)
  add(key, "throughput", $19)
  add(key, "delay", $20)
  add(key, "tearing", $22)
  add(key, "outOfRange", $23)
  add(key, "rxPackets", $13)
  add(key, "rxBytes", $14)
  add(key, "rxPayloadBytes", $15)
  add(key, "lostPackets", $16)
}
END {
  print "mode,nNodes,runs,clients,rtsCtsThreshold,packetSize,packetRate,simTime,serverStart,clientStart,offeredLoadMbps,pdrMean,pdrStd,lossRateMean,lossRateStd,throughputMeanMbps,throughputStdMbps,avgDelayMeanMs,avgDelayStdMs,linkTearingMeanPerSecond,linkTearingStdPerSecond,outOfRangeMeanPercent,outOfRangeStdPercent,rxPacketsMean,rxBytesMean,rxPayloadBytesMean,lostPacketsMean"
  for (i = 1; i <= keyCount; ++i) {
    key = keys[i]
    printf "%s,%s,%d,%s,%s,%s,%s,%s,%s,%s,%s,%.6f,%.6f,%.6f,%.6f,%.9f,%.9f,%.6f,%.6f,%.9f,%.9f,%.6f,%.6f,%.3f,%.3f,%.3f,%.3f\n",
      mode[key], nodes[key], count[key], clients[key], rts[key], packetSize[key],
      packetRate[key], simTime[key], serverStart[key], clientStart[key], offered[key],
      avg(key, "pdr"), stddev(key, "pdr"),
      avg(key, "loss"), stddev(key, "loss"),
      avg(key, "throughput"), stddev(key, "throughput"),
      avg(key, "delay"), stddev(key, "delay"),
      avg(key, "tearing"), stddev(key, "tearing"),
      avg(key, "outOfRange"), stddev(key, "outOfRange"),
      avg(key, "rxPackets"), avg(key, "rxBytes"), avg(key, "rxPayloadBytes"),
      avg(key, "lostPackets")
  }
}
' "$rawSummary" > "$aggSummary"

awk -F, -v OFS=',' '
function add(key, name, value) {
  sum[key, name] += value
  sumsq[key, name] += value * value
}
function avg(key, name) {
  return sum[key, name] / count[key]
}
function stddev(key, name, n, mean, variance) {
  n = count[key]
  if (n < 2) {
    return 0
  }
  mean = sum[key, name] / n
  variance = (sumsq[key, name] - n * mean * mean) / (n - 1)
  if (variance < 0 && variance > -0.000000001) {
    variance = 0
  }
  return sqrt(variance)
}
NR == 1 {
  next
}
{
  key = $1 SUBSEP $2 SUBSEP $8
  if (!(key in seen)) {
    seen[key] = 1
    keys[++keyCount] = key
    mode[key] = $1
    nodes[key] = $2
    intervalEnd[key] = $8
    activeClients[key] = $9
    offered[key] = $17
  }
  count[key]++
  add(key, "pdr", $15)
  add(key, "loss", $16)
  add(key, "throughput", $18)
  add(key, "delay", $19)
  add(key, "tearing", $21)
  add(key, "outOfRange", $22)
  add(key, "rxBytes", $12)
  add(key, "rxPayloadBytes", $13)
}
END {
  print "mode,nNodes,intervalEnd,runs,activeClients,offeredLoadMbps,pdrMean,pdrStd,lossRateMean,lossRateStd,throughputMeanMbps,throughputStdMbps,avgDelayMeanMs,avgDelayStdMs,linkTearingMeanPerSecond,linkTearingStdPerSecond,outOfRangeMeanPercent,outOfRangeStdPercent,rxBytesMean,rxPayloadBytesMean"
  for (i = 1; i <= keyCount; ++i) {
    key = keys[i]
    printf "%s,%s,%s,%d,%s,%s,%.6f,%.6f,%.6f,%.6f,%.9f,%.9f,%.6f,%.6f,%.9f,%.9f,%.6f,%.6f,%.3f,%.3f\n",
      mode[key], nodes[key], intervalEnd[key], count[key], activeClients[key], offered[key],
      avg(key, "pdr"), stddev(key, "pdr"),
      avg(key, "loss"), stddev(key, "loss"),
      avg(key, "throughput"), stddev(key, "throughput"),
      avg(key, "delay"), stddev(key, "delay"),
      avg(key, "tearing"), stddev(key, "tearing"),
      avg(key, "outOfRange"), stddev(key, "outOfRange"),
      avg(key, "rxBytes"), avg(key, "rxPayloadBytes")
  }
}
' "$rawMilestones" > "$aggMilestones"

cat <<EOF
DONE
rawSummary=${rawSummary}
rawMilestones=${rawMilestones}
aggSummary=${aggSummary}
aggMilestones=${aggMilestones}
log=${logFile}
xmlDir=${xmlDir}
EOF
