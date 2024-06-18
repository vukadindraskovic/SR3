const mqtt = require('mqtt');
const { InfluxDB, Point } = require('@influxdata/influxdb-client');

const mqttAddress = 'tcp://emqx:1883'
const clientId = 'iot-application'
const username = 'iot-application'
const password = 'iot-application'
const topic = 'serial-reader/data'

const url = 'http://influxdb:8086';
const token = 'MkoqBU1IRStTweJnx0uM1BfNTDqtId3Rofx-BqARS_HJjdbJzt_DzW0pSyI1vh-7smlN1bKixZNt0s02izfuFA==';
const organization = 'elfak';
const bucket = 'sr3';

const influxdb = new InfluxDB({ url, token });

const writeApi = influxdb.getWriteApi(organization, bucket);

const mqttClient = mqtt.connect(mqttAddress, {
    clientId,
    connectTimeout: 4000,
    username: username,
    password: password,
    reconnectPeriod: 1000
})

mqttClient.on('connect', () => {
    console.log('Connected')
    mqttClient.subscribe(topic, () => {
        console.log(`Subscribed to topic '${topic}'`)
    })
})

mqttClient.on('message', (topic, payload) => {
    const json = JSON.parse(payload.toString())
    console.log(`Received Message from '${topic}': `, json)
    
    const point = new Point('temperature-measurement')
      .floatField('value', parseFloat(json))
      .timestamp(new Date());
  
    writeApi.writePoint(point);
    writeApi.flush();
})

process.on('exit', () => {
    writeApi.close().then(() => {
      console.log('WRITE API closed');
    });
  });