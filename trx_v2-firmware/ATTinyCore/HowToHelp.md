# How you can help ATTinyCore

## Report Bugs
Please report bugs! That's how things get fixed - I am almost never aware of a problem until it is reported. Use the GitHub issues system, preferably. If not that, either via email (spencekonde@gmail.com) or via post on the Arduino forum, Microcontrollers section. **The objective of these cores is to be the most thoroughly featured, best functioning core possible** for all supported parts.

## Fix Bugs and PR
If you have already fixed a bug in my core for yourself, you can submit those changes via github as a "pull request". If you try to "edit" a file in this repo (and you're not me), it will automatically create a fork in your github account, where you can make changes, and then send them back to me as a PR. Just please don't forget to do that last step :-) I am also very happy to accept fixes via email or any other mode of communication short of wrapping them around bricks and throwing them through my window. Github just very clearly gives you credit for the submission :-)

Note that the above includes spelling/grammar check - I write these in code editors without spell check, and I have a tendency to change how I structure sentences halfway through writing them, and not notice.

## Library Listing
There are open issues on ATTinyCore and megaTinyCore for information on which libraries are and are not compatible. Thus far, I have not gotten any feedback on this, which is disappointing. It would be, I think, a great help to users if there were a list of which libraries do and do not work with these cores (and, for cases where they do not work - any information on alternatives to those libraries). For libraries that are particular priorities, I can intervene to try to add support or get support added (I recently did that with OneWire for the megaTinyCore parts (as well as non-4809 megaAVR 0-series and new Dx-series). Similarly, if you're a library designer wondering how to make their libraries work with any of my cores, I am more than happy to assist!

## Spread the word
They say that "big news travels fast" - tinyNews needs all the help it can get. Tell your maker friends about ATTinyCore, megaTinyCore, and DxCore. There are a lot of tutorials and guides out there that point people to other cores, cores which are now minimally maintained, if they are maintained at all - so there's plenty of word-spreading to be done. The more people using ATTinyCore, the more people will be finding, reporting and/or fixing bugs, and the faster ATTinyCore will get better - so everyone wins here.

## Last but by no means least - give me money
I will be launching a Patreon in the near future. Additionally, you can support me buy buying things from my Tindie store. Everyone needs prototyping board - and even if **you** don't, they make a great gift for your loved ones who like electronics! And, for that matter, they're also great gifts for your unloved ones who don't do anything with electronics.

## Rewards?
While I can't promise anything beyond recognition for pull requests or other contributions - if you submit a particularly large or impactful contribution. that's a good way to get some free stuff from my Tindie store ;-) Which, naturally, is focused on things relating to the parts my cores supports, so it should be of particular use to you.

Two things are guaranteed to be worth a reward:
* A demonstration of how to use the avr_gcc function attribute "warning" to produce a warning if and only if a function declared with that attribute is **reachable** (this is supposed to work like #error, which generates an error only if the function declared with the error attribute is reachable after preprocessing, link time optimization, and constant folding. This is very different from the #warning directive, which generated a warning if it is present anywhere in the code after preprocessing.
* An investigation of the ADC on the tragic ATtiny828, because I am dying of curiosity. See [my unsolved mysteries page on it](https://github.com/SpenceKonde/AVR_Research/blob/main/UnsolvedMysteries.md#the-828-and-the-mystery-of-the-adc) - in short, there is extremely strong evidence to suggest that there is a very fancy differential ADC that was planned for that part, and scrubbed at the last moment, with signs of a hasty editing of the datasheet to remove refeences to it. The register layout is identical to the Tiny841's layout, and it had a VERY fancy differential ADC. Based on the huge number of pins with analog inputs, it seems like the ADC was to be the crown jewel of the 828. My theory is that the window for release of the last classic tinyAVRs was closing, and it came back from the fab and the ADC was busted. The engineers asked for a respin, and management said "no", and so they released it was is. I speculate that they would not have released it at all if there was not *some* semblance of the differential ADC functionality there - that there was a workaround - but it was too complicated or too embarrassing to describe in the errata; thus certain friendly customers (maybe ones involved in planning the part) know the trick to make the ADC run (or at least limp) in differential mode. I would love to close thebook on this with either a conclusive proof that no functionality at all existed there, or information on the state of the ADC in production silicon.