# Bragging

Author: 
Xiaoqiao Xu, Zizhuo Lin

Design: 
Each player has 6 dices and one of the player will make a claim on how many dices with certain face up are presented. The other player will have the choice to whether reveal the true state or make a new claim.

Networking: 
The Connection interface provided is used to transmit state messages. User action and required game state message is transmitted such as the dice state for a user and the claim other user made.
Currently only supports 2 player at a time.

Screen Shot:

![Screen Shot](screenshot.png)

How To Play:
Controls of each step is introduced in the game. The strategy is to compare the claim and your dices to decide whether the claim is valid and reveal the true state if you think is invalid. If you think the claim is valid, you need to make a new claim back.

Sources: (TODO: list a source URL for any assets you did not create yourself. Make sure you have a license for the asset.)

This game was built with [NEST](NEST.md).

