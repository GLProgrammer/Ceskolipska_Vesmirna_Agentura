#!/usr/bin/python3

# Uses examples from: https://www.python-course.eu/tkinter_entry_widgets.php

import tkinter as tk
import os
import sys
import subprocess
import webbrowser

config_name = "config"
is_server_running = False

# Load config file
if os.path.exists(config_name):
    with open(config_name, 'r', encoding = 'utf-8') as config:
        name = config.readline()[:-1]
        latitude = config.readline()[:-1]
        longitude = config.readline()[:-1]
        frequency = config.readline()[:-1]
        gain = config.readline()[:-1]
else:
    name = "(sem zadejte vaši značku)"
    latitude = "50.083333"
    longitude = "14.416667"
    frequency = "434.000"
    gain = "5"

print("From config:")
print("\tCall sign: ", name);
print("\tLatitude: ", latitude);
print("\tLongitude: ", longitude);
print("\tFrequency: ", frequency);
print("\tGain: ", gain);

# Save data to config and run command
def start_button_handler():
    global is_server_running
    global server_process
    name = e1.get()
    latitude = e2.get().split('N')[0]
    longitude = e3.get().split('E')[0]
    frequency = e4.get()
    gain = e5.get()
    
    print("From user:")
    print("\tCall sign: ", name);
    print("\tLatitude: ", latitude);
    print("\tLongitude: ", longitude);
    print("\tFrequency: ", frequency);
    print("\tGain: ", gain);
    
    with open(config_name, 'w', encoding = 'utf-8') as config:
        config.write(name + '\n');
        config.write(latitude + '\n');
        config.write(longitude + '\n');
        config.write(frequency + '\n');
        config.write(gain + '\n');

    if is_server_running:
        server_process.terminate()
    else:
        is_server_running = True
        start_button_text.set("Restart")

    server_process = subprocess.Popen(["./habdecWebsocketServer", "--device", "0", "--sampling_rate", "2.024e6", "--rtty", "300", "7", "2", "-print", "1", "--freq", frequency, "--gain", gain, "--biast", "0", "--afc", "0", "--station", name, "--latlon", latitude, longitude])

# Open web browser
def browser_button_handler():
    webbrowser.open('file:///home/lubuntu/DSV/habdec/code/webClient/index.html')

# Create window
master = tk.Tk()
master.title('Přijímač DSV')

# Fields labels
tk.Label(master, text="Značka").grid(row=0, column=0, pady=2)
tk.Label(master, text="Zeměpisná šířka").grid(row=1, column=0, pady=2)
tk.Label(master, text="Zeměpisná délka").grid(row=2, column=0, pady=2)
tk.Label(master, text="Frekvence [MHz]").grid(row=3, column=0, pady=2)
tk.Label(master, text="Zisk").grid(row=4, column=0, pady=2)

# Input fields
e1 = tk.Entry(master)
e2 = tk.Entry(master)
e3 = tk.Entry(master)
e4 = tk.Entry(master)
e5 = tk.Entry(master)

e1.grid(row=0, column=1)
e2.grid(row=1, column=1)
e3.grid(row=2, column=1)
e4.grid(row=3, column=1)
e5.grid(row=4, column=1)

e1.insert(10, name);
e2.insert(10, latitude);
e3.insert(10, longitude);
e4.insert(10, frequency);
e5.insert(10, gain);

# Button
start_button_text = tk.StringVar()
start_button_text.set("Start")
tk.Button(master, textvariable=start_button_text, command=start_button_handler).grid(row=6, column=0, sticky=tk.W, pady=10)
tk.Button(master, text='Web', command=browser_button_handler).grid(row=6, column=1, sticky=tk.W, pady=10)

master.mainloop()

if 'server_process' in globals():
	server_process.terminate()

