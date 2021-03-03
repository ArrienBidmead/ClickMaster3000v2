//ClickMaster 3000 v2 - Auto clicker tool for use with clicker games
//Copyright(C) 2021  Arrien Bidmead - Arrien.co.uk
//
//This program is free software : you can redistribute it and /or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program. If not, see <https://www.gnu.org/licenses/>.

#include <SFML/Graphics.hpp>
#include "Button.h"
#include "Textures.h"
#include <iostream>
#include <Windows.h>
#include <Dwmapi.h>
#include <thread>
#include "BinaryTree.h"

#include "Source.h"

#if !_DEBUG
	#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")	// Disable console
#endif

void CM3000v2::Init()
{
	Textures textures;

	sf::Image ic;
	ic.loadFromMemory(textures.Iconpng, sizeof(textures.Iconpng));
	window.setIcon(ic.getSize().x, ic.getSize().y, ic.getPixelsPtr());

	overlapTex.loadFromMemory(textures.ButtonOverlap1png, sizeof(textures.ButtonOverlap1png));
	whiteTex.loadFromMemory(textures.WhiteSquarepng, sizeof(textures.WhiteSquarepng));

	backTex.loadFromMemory(textures.OuterBox1png, sizeof(textures.OuterBox1png));
	backgroundButton.baseSprite.setTexture(backTex, true);
	backgroundButton.setPosition(0, 0);
	backgroundButton.baseColor = { 128, 128, 128, 255 };
	backgroundButton.bStaticAppearance = true;
	backgroundButton.OnPressBinding = [&]()
	{
		dragMousePos = sf::Mouse::getPosition(window);
		bDraggingWindow = true;
	};

	buttonsRect.baseSprite.setTexture(whiteTex, true);
	buttonsRect.baseColor = sf::Color(10, 10, 10, 255);
	buttonsRect.setPosition(8, 8);
	buttonsRect.bStaticAppearance = true;
	buttonsRect.setScale(sf::Vector2f(8 * 3, 8));
	buttonsRect.OnPressBinding = [&]()
	{
		bDraggingWindow = false;
	};

	armTex.loadFromMemory(textures.ArmButton1png, sizeof(textures.ArmButton1png));
	armButton.SetTextures(&armTex, &overlapTex);
	armButton.setPosition(8, 8);
	armButton.baseColor = { 180, 180, 180, 255 };
	armButton.hoverColor = { 30, 140, 30, 255 };
	armButton.OnReleaseBinding = [&]()
	{
		bArmed = !bArmed;

		if (bArmed)
		{
			armButton.baseColor = { 0, 255, 0, 255 };
			armButton.hoverColor = { 170, 255, 170, 255 };

			clickThread = std::thread([&]()
			{
				while (bArmed)
				{
					if (bActive)
					{
						unsigned char timing = int(float(cpsDowntime) * 0.5);
						mouse_event(MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_ABSOLUTE, MOUSEEVENTF_ABSOLUTE, 0, 0);
						Sleep(timing);
						mouse_event(MOUSEEVENTF_LEFTUP, MOUSEEVENTF_ABSOLUTE, MOUSEEVENTF_ABSOLUTE, 0, 0);
						Sleep(timing);
					}
					else
					{
						Sleep(8);
					}
				}
			});
		}
		else
		{
			armButton.baseColor = { 180, 180, 180, 255 };
			armButton.hoverColor = { 30, 140, 30, 255 };

			clickThread.join();	//Could be detach() but I don't have faith it will be terminated
		}

		armButton.UpdateColors();
	};

	cpsTex.loadFromMemory(textures.CPSButton0png, sizeof(textures.CPSButton0png));
	cpsTex1.loadFromMemory(textures.CPSButton1png, sizeof(textures.CPSButton1png));
	cpsTex2.loadFromMemory(textures.CPSButton2png, sizeof(textures.CPSButton2png));
	cpsButton.SetTextures(&cpsTex1, &overlapTex);
	cpsButton.setPosition(overlapTex.getSize().x + 8, 8);
	cpsButton.baseColor = { 180, 180, 180, 255 };
	cpsButton.hoverColor = { 140, 100, 30, 255 };
	cpsButton.OnReleaseBinding = [&]()
	{
		if (cpsState < 2)
			cpsState++;
		else
			cpsState = 0;

		switch (cpsState)
		{
		case 0:
			cpsButton.baseSprite.setTexture(cpsTex);
			cpsDowntime = 32;
			break;
		case 1:
			cpsButton.baseSprite.setTexture(cpsTex1);
			cpsDowntime = 16;
			break;
		case 2:
			cpsButton.baseSprite.setTexture(cpsTex2);
			cpsDowntime = 8;
			break;
		}
	};

	exitTex.loadFromMemory(textures.ExitButton1png, sizeof(textures.ExitButton1png));
	exitButton.SetTextures(&exitTex, &overlapTex);
	exitButton.setPosition(overlapTex.getSize().x * 2 + 8, 8);
	exitButton.baseColor = { 180, 180, 180, 255 };
	exitButton.hoverColor = { 140, 30, 30, 255 };
	exitButton.OnReleaseBinding = [&]()
	{
		window.close();
	};

	//Binary Tree
	/*The binary tree is used for optimisation, a child node will only be polled/updated if the mouse cursor is hovering over the parent node.
	For example, the 3 main buttons (arm,cps,exit - all siblings of each other) will only get updated if the mouse is hovering over the parent buttonsRect.*/
	BinaryNode<Button*>* curNode = buttonsTree.AddHeadNode(&backgroundButton);
	curNode = buttonsTree.AddChildNodeTo(curNode, &buttonsRect);
	curNode = buttonsTree.AddChildNodeTo(curNode, &armButton);
	curNode = buttonsTree.AddSiblingNodeTo(curNode, &cpsButton);
	curNode = buttonsTree.AddSiblingNodeTo(curNode, &exitButton);

	buttonsTree.PreOrderTraverse(buttonsTree.head, [&](BinaryNode<Button*>* node)
	{
		node->data->FinalizeInit();
	});
}

void CM3000v2::UpdateTraverse(BinaryNode<Button*>* node, bool& bShouldReDraw)
{
	node->data->Poll(window);
	bShouldReDraw |= node->data->bShouldReDraw;

	if (node->right != nullptr)
	{
		if (node->data->bHovered)
		{
			UpdateTraverse(node->right, bShouldReDraw);
		}
		else
		{
			//Check to see if any node deeper in the tree needs to be updated because it is in a hovered state
			bool bContinue = false;
			buttonsTree.PreOrderTraverse(node->right, [&](BinaryNode<Button*>* node)
			{
				bContinue |= node->data->bHovered;
			});

			if (bContinue)
				UpdateTraverse(node->right, bShouldReDraw);
		}
	}

	if (node->left != nullptr)
		UpdateTraverse(node->left, bShouldReDraw);
}

bool CM3000v2::Update()
{
	bool bDraw = false;
	UpdateTraverse(buttonsTree.head, bDraw);

	if (bDraggingWindow)
	{
		if (sf::Mouse::isButtonPressed(sf::Mouse::Left))
		{
			window.setPosition(sf::Mouse::getPosition() - dragMousePos);
		}
		else
		{
			bDraggingWindow = false;
		}
	}

	if (bActive)
	{
		if (!bArmed || !GetKeyState(VK_CAPITAL))
		{
			bActive = false;
			backgroundButton.baseColor = sf::Color(128, 128, 128, 255);
			backgroundButton.UpdateColors();
		}
	}
	else
	{
		if (bArmed && GetKeyState(VK_CAPITAL))
		{
			bActive = true;
			backgroundButton.baseColor = sf::Color(255, 0, 0, 255);
			backgroundButton.UpdateColors();
		}
	}

	return bDraw;
}

void CM3000v2::Draw()
{
	buttonsTree.PreOrderTraverse(buttonsTree.head, [&](BinaryNode<Button*>* node)
	{
		node->data->Draw(&window);
	});
}

void CM3000v2::Cleanup()
{
	if (bArmed)
	{
		bArmed = false;
		clickThread.join();
	}

	buttonsTree.PostOrderTraverse(buttonsTree.head, [&](BinaryNode<Button*>* node)
	{
		delete node;
	});
}

int CM3000v2::Run()
{
	window.create(sf::VideoMode(208, 80), "ClickMaster 3000", sf::Style::None);

	MARGINS margins;
	margins.cxLeftWidth = -1;
	HWND hwnd = window.getSystemHandle();

	SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
	DwmExtendFrameIntoClientArea(hwnd, &margins);

	SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);	//locks app in front of everything else

	Init();

	sf::Clock clock;
	while (window.isOpen())
	{
		if (!bDraggingWindow)
		{
			sf::sleep(sf::milliseconds(2));

			if (clock.getElapsedTime().asMilliseconds() < 16)
				continue;
			else
				clock.restart();
		}
		else
		{
			clock.restart();
		}

		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
		}

		if (Update())
		{
			window.clear(sf::Color::Transparent);

			Draw();

			window.display();
		}
	}

	Cleanup();

	return 0;
}

int main()
{
	CM3000v2 program;
	return program.Run();
}