# Cat Dog Blinker

The idea of this app is to test and get familiarised with the built-in API provided by Google Coral Team. There are several obstacles making this work but it's finally coming into place. 

## What I've Done

First of all, I made a LED Blinking app to see whether everything is functioning correctly. I found out that using Type-C to Type-C cable works but not USB-A to Type-C for reasons I don't understand. Then, I tried to use ChatGPT to come up with a solution to deploy a tflite model onto the board, which I failed to do so. Therefore, I revert back to this version where I try to stream camera onto my linux machine by referring to the examples. At least now I know I can see what the camera see and it will likely help identify whether a model is working correctly. 

## How to execute the app? 

First, have a linux machine (I have an ubuntu 22 on a think pad), a Type-C cable, and a coral micro dev board. You don't have to plug in initially. 

Now open up a terminal and clone a repository into your machine: 

```
git clone https://github.com/daniel7722/coralmicro.git
```

or you can fork it from my github account.

Then, you need to run: 

```
cd coralmicro (or whatever you named your cloned repo)
bash setup.sh
bash build.sh
```

The nitty gritty of this particular repo is that I only succeeded at setting things up with a python virtual env of version 3.8.10. Any python version higher than that would likely results in outdated libraries that are complicated to download. 

If everything works, it means that you can flash all of the apps or examples onto the board. To start any app, you need to plug in the board first and you should probably see a orange LED being turned on. run `lsusb` to see if your computer can find the device. 

Now you can flash through: 
```
python3 scripts/flashtool.py -a <app name> (or -e <example name>)
<!-- To run this app, please run the following line -->
python3 scripts/flashtool.py -a catdog_blinker
```

Then, you should see the terminal saying it's restarting the device. After seeing this message, run the stream.py python file to activate camera streaming on your computer. 

```
python3 apps/catdog_blinker/stream.py
```
Now you should be able to see the streaming camera and Voila. 



## Next Step
I will try to deploy a vision model and see if this board is able to process streaming data and make decision upon that. Stay tuned. 