"""This module provides the Command Cli."""
# k2egcli/command.py

import typer
import json
from k2egcli import producer
import uuid
from kafka import KafkaConsumer

app = typer.Typer()


@app.command()
def get(
    kafka_host: str = typer.Option(
        "kafka:9092",
        "--kafka",
        "-k",
        help="Is the hostname of kafka boostrap server",
    ),
    cmd_topic: str = typer.Option(
        "cmd_topic_in",
        "--cmd-topic",
        "-ct",
        help="Is the k2eg command input topic",
    ),
    channel_name: str = typer.Option(
        ...,
        "--channel-name",
        "-n",
        help="Is the name of the channel(pv) target of get operation",
    ),
    channel_protocol: str = typer.Option(
        "pva",
        "--channel-protocol",
        "-p",
        help="Is the epics protocol of the channel(pv) target of get operation",
    ),
    destination_topic: str = typer.Option(
        "data_topic_out",
        "--destintion-topic",
        "-d",
        help="Is the destination topic where to send the result",
    ),
    seriailization_type: str = typer.Option(
        "json",
        "--serialization",
        "-s",
        help="Is the srializaiotn type of the output message",
    ),
    listen_for_event: bool = typer.Option(
        "false",
        "--listen",
        "-l",
        help="Listen for the event",
    ),
):
    print(f"Using kafka host: [bold green]{kafka_host}[/bold green]")
    print(f"Using command topic: [bold green]{cmd_topic}[/bold green]")
    get_command = {
        "command": "get",
        "serialization": seriailization_type,
        "protocol": channel_protocol,
        "channel_name": channel_name,
        "dest_topic": destination_topic
    }

    print(f"Send command: [bold green]{get_command}[/bold green]")
    kp = producer.Producer(kafka_host)
    kp.send(cmd_topic, json.dumps(get_command).encode('utf-8'))
    if liste_for_event:
        ks = KafkaConsumer(
            bootstrap_servers=[kafka_host],
            auto_offset_reset="latest",
            enable_auto_commit=False,
            group_id='k2eg_'+str(uuid.uuid1())
        )
        ks.subscribe([destination_topic])
        try:
            for message in ks:
                ks.commit()
                message = f"""
                Message received: {message.value}
                Message key: {message.key}
                Message partition: {message.partition}
                Message offset: {message.offset}
                """
                print(message)
                break
        except KeyboardInterrupt:
            print('stop listening!')
        
        ks.close()
        kp.close()

@app.command()
def start_monitor(
    kafka_host: str = typer.Option(
        "kafka:9092",
        "--kafka",
        "-k",
        help="Is the hostname of kafka boostrap server",
    ),
    cmd_topic: str = typer.Option(
        "cmd_topic_in",
        "--cmd-topic",
        "-ct",
        help="Is the k2eg command input topic",
    ),
    channel_name: str = typer.Option(
        ...,
        "--channel-name",
        "-n",
        help="Is the name of the channel(pv) target of monitor operation",
    ),
    channel_protocol: str = typer.Option(
        "pva",
        "--channel-protocol",
        "-p",
        help="Is the epics protocol of the channel(pv) target of monitor operation",
    ),
    destination_topic: str = typer.Option(
        "data_topic_out",
        "--destintion-topic",
        "-d",
        help="Is the destination topic where to send the result",
    ),
    seriailization_type: str = typer.Option(
        "json",
        "--serialization",
        "-s",
        help="Is the srializaiotn type of the output message",
    ),
    listen_for_event: bool = typer.Option(
        "false",
        "--listen",
        "-l",
        help="Listen for the event",
    ),
):
    print(f"Using kafka host: [bold green]{kafka_host}[/bold green]")
    print(f"Using command topic: [bold green]{cmd_topic}[/bold green]")
    start_monitor_command = {
        "command": "monitor",
        "serialization": seriailization_type,
        "protocol": channel_protocol,
        "channel_name": channel_name,
        "dest_topic": destination_topic,
        "activate": True
    }

    print(f"Send command: [bold green]{start_monitor_command}[/bold green]")
    kp = producer.Producer(kafka_host)
    kp.send(cmd_topic, json.dumps(start_monitor_command).encode('utf-8'))
    if liste_for_event:
        ks = KafkaConsumer(
            bootstrap_servers=[kafka_host],
            auto_offset_reset="latest",
            enable_auto_commit=False,
            group_id='k2eg_'+str(uuid.uuid1())
        )
        ks.subscribe([destination_topic])
        try:
            for message in ks:
                ks.commit()
                message = f"""
                Message received: {message.value}
                Message key: {message.key}
                Message partition: {message.partition}
                Message offset: {message.offset}
                """
                print(message)
        except KeyboardInterrupt:
            print('stop listening!')

        
        ks.close()
        kp.close()

@app.command()
def stop_monitor(
    kafka_host: str = typer.Option(
        "kafka:9092",
        "--kafka",
        "-k",
        help="Is the hostname of kafka boostrap server",
    ),
    cmd_topic: str = typer.Option(
        "cmd_topic_in",
        "--cmd-topic",
        "-ct",
        help="Is the k2eg command input topic",
    ),
    channel_name: str = typer.Option(
        ...,
        "--channel-name",
        "-n",
        help="Is the name of the channel(pv) target of monitor operation",
    ),
    channel_protocol: str = typer.Option(
        "pva",
        "--channel-protocol",
        "-p",
        help="Is the epics protocol of the channel(pv) target of monitor operation",
    ),
    destination_topic: str = typer.Option(
        "data_topic_out",
        "--destintion-topic",
        "-d",
        help="Is the destination topic where to send the result",
    ),
):
    print(f"Using kafka host: [bold green]{kafka_host}[/bold green]")
    print(f"Using command topic: [bold green]{cmd_topic}[/bold green]")
    start_monitor_command = {
        "command": "monitor",
        "channel_name": channel_name,
        "dest_topic": destination_topic,
        "activate": False
    }

    print(f"Send command: [bold green]{start_monitor_command}[/bold green]")
    kp = producer.Producer(kafka_host)
    kp.send(cmd_topic, json.dumps(start_monitor_command).encode('utf-8'))
    kp.close()
