#!/usr/bin/env python3
"""Gravador de log da porta serial do Lummina4Ed.

Uso:
    serial_logger.exe COM7
    serial_logger.exe COM7 --baud 115200 --outfile meu_log.log

Sem argumentos, lista as portas disponiveis e pede pra escolher uma.
Grava tudo que a porta serial mandar num arquivo .log com timestamp por
linha, e reconecta sozinho se a porta cair (ex: equipamento reiniciando
ou perdendo energia durante um teste longo em autoclave).
"""

import argparse
import datetime
import re
import sys
import time

import serial
import serial.tools.list_ports

ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;]*m")


def choose_port() -> str:
    ports = list(serial.tools.list_ports.comports())

    if not ports:
        print("Nenhuma porta serial encontrada.")
        sys.exit(1)

    print("Portas disponiveis:")
    for i, p in enumerate(ports):
        print(f"  [{i}] {p.device} - {p.description}")

    while True:
        choice = input("Escolha o numero da porta: ").strip()
        if choice.isdigit() and 0 <= int(choice) < len(ports):
            return ports[int(choice)].device
        print("Opcao invalida, tenta de novo.")


def default_outfile(port: str) -> str:
    ts = datetime.datetime.now().strftime("%Y-%m-%d_%H%M%S")
    safe_port = port.replace("/", "_").replace("\\", "_")
    return f"monitor_{safe_port}_{ts}.log"


def run(port: str, baud: int, outfile: str) -> None:
    print(f"Porta: {port}  Baud: {baud}")
    print(f"Gravando em: {outfile}")
    print("Ctrl+C pra parar.\n")

    with open(outfile, "a", encoding="utf-8", errors="replace") as f:
        f.write(f"\n--- Log iniciado em {datetime.datetime.now().isoformat()} "
                 f"(porta {port}, {baud} bps) ---\n")
        f.flush()

        while True:
            try:
                with serial.Serial(port, baud, timeout=1) as ser:
                    f.write(f"--- Conectado em {datetime.datetime.now().isoformat()} ---\n")
                    f.flush()

                    while True:
                        raw = ser.readline()
                        if not raw:
                            continue

                        line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                        line = ANSI_ESCAPE.sub("", line)
                        stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
                        out_line = f"[{stamp}] {line}"

                        print(out_line)
                        f.write(out_line + "\n")
                        f.flush()

            except KeyboardInterrupt:
                f.write(f"--- Log encerrado pelo usuario em "
                        f"{datetime.datetime.now().isoformat()} ---\n")
                print("\nParado pelo usuario.")
                return

            except serial.SerialException as e:
                f.write(f"--- Porta caiu ({e}) - tentando reconectar em "
                        f"{datetime.datetime.now().isoformat()} ---\n")
                f.flush()
                print(f"Porta caiu ({e}) - tentando reconectar...")
                time.sleep(2)


def main() -> None:
    parser = argparse.ArgumentParser(description="Gravador de log serial do Lummina4Ed")
    parser.add_argument("port", nargs="?", default=None,
                         help="Porta serial (ex: COM7). Se omitido, lista as portas disponiveis.")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (padrao 115200)")
    parser.add_argument("--outfile", default=None,
                         help="Arquivo de log (padrao: gera nome automatico com data/hora)")

    args = parser.parse_args()

    port = args.port or choose_port()
    outfile = args.outfile or default_outfile(port)

    try:
        run(port, args.baud, outfile)
    except KeyboardInterrupt:
        print("\nParado pelo usuario.")


if __name__ == "__main__":
    main()
