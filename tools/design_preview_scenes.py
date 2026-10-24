from __future__ import annotations

from PIL import Image, ImageDraw


def pixel_scene_source(route: str) -> Image.Image:
    from random import Random

    image = Image.new("RGB", (228, 135), "#173B34")
    d = ImageDraw.Draw(image)

    def noise(box, colors, count, seed):
        rng = Random(seed)
        for _ in range(count):
            x = rng.randint(box[0], box[2] - 1)
            y = rng.randint(box[1], box[3] - 1)
            d.point((x, y), fill=colors[rng.randrange(len(colors))])

    def grass(x, y, color):
        for dx, dy in [(-5, -7), (-2, -10), (2, -8), (5, -5)]:
            d.line((x, y, x + dx, y + dy), fill=color)
        d.line((x - 4, y - 4, x + 3, y - 2), fill=color)

    def crystal(x, y, height, color):
        d.polygon([(x - 4, y), (x, y - height), (x + 4, y), (x + 1, y + 3)], fill=color)
        d.line((x, y - height + 3, x, y - 2), fill="#C4D8CD")
        d.polygon([(x - 9, y + 2), (x - 7, y - height // 2), (x - 3, y + 1)], fill="#7185A0")
        d.polygon([(x + 3, y + 1), (x + 7, y - height * 2 // 3), (x + 10, y + 2)], fill=color)

    if route == "forest":
        d.rectangle((0, 0, 227, 134), fill="#173C36")
        d.rectangle((174, 14, 184, 24), fill="#D8CB8B")
        d.rectangle((176, 12, 182, 26), fill="#D8CB8B")
        d.polygon([(0, 49), (28, 25), (50, 43), (79, 19), (111, 46),
                   (145, 27), (178, 45), (207, 27), (227, 44), (227, 64), (0, 64)], fill="#2A5347")
        noise((0, 38, 228, 62), ["#365D4D", "#2A5347", "#416553"], 150, 11)
        for x in range(-5, 235, 13):
            height = 11 + (x * 7 % 9)
            d.polygon([(x, 66), (x + 6, 66 - height), (x + 12, 66)], fill="#204834")
        d.rectangle((0, 63, 227, 134), fill="#2D5038")
        d.polygon([(94, 134), (142, 134), (128, 105), (126, 83), (118, 62),
                   (109, 73), (105, 96)], fill="#817B52")
        d.polygon([(105, 134), (130, 134), (121, 103), (120, 82), (116, 70),
                   (112, 86), (112, 108)], fill="#A29663")
        d.polygon([(105, 134), (130, 134), (126, 113), (126, 107), (118, 104),
                   (109, 105), (103, 113)], fill="#817B52")
        d.ellipse((105, 104, 124, 110), fill="#A29663")
        d.ellipse((99, 106, 130, 111), fill="#817B52")
        noise((96, 84, 140, 134), ["#B4A672", "#6C6B47", "#8E875A"], 90, 12)
        for x, y, height in [(8, 43, 66), (37, 55, 48), (190, 43, 69), (219, 55, 55), (65, 64, 35), (163, 65, 34)]:
            d.rectangle((x - 3, y, x + 3, y + height), fill="#3D4630")
            d.rectangle((x, y + 4, x + 2, y + height - 5), fill="#675E3D")
            d.line((x, y + 18, x - 10, y + 8), fill="#4B4D32", width=2)
            d.line((x + 1, y + 27, x + 11, y + 15), fill="#4B4D32", width=2)
            for dx, dy, w, color in [(-13, -9, 23, "#173D2D"), (0, -17, 25, "#2E6845"),
                                     (-7, -25, 17, "#4A7C50"), (8, -7, 18, "#245537")]:
                d.ellipse((x + dx - w // 2, y + dy - 7, x + dx + w // 2, y + dy + 7), fill=color)
            noise((x - 18, y - 28, x + 19, y - 5), ["#396F47", "#568557", "#234D35"], 24, x)
        for x, y in [(12, 113), (42, 124), (181, 116), (211, 126), (67, 99), (160, 95)]:
            grass(x, y, "#7EA45B")
            grass(x + 8, y + 3, "#4F7B47")
        for x, y, w in [(24, 127, 18), (177, 130, 23), (58, 113, 12)]:
            d.arc((x, y - 3, x + w, y + 3), 180, 355, fill="#796543", width=2)
        for x, y in [(81, 44), (147, 37), (179, 79), (48, 85), (93, 68)]:
            d.point((x, y), fill="#D8D98D")
            if (x + y) % 2:
                d.point((x + 1, y), fill="#8FA76B")
    elif route == "coast":
        d.rectangle((0, 0, 227, 134), fill="#1C4255")
        for x, y, w in [(15, 17, 39), (142, 12, 47), (78, 32, 30), (190, 34, 25)]:
            d.rectangle((x, y, x + w, y + 4), fill="#6D8E99")
            d.rectangle((x + 7, y - 4, x + w - 9, y), fill="#86A3AA")
            d.rectangle((x + 8, y - 7, x + w - 8, y - 4), fill="#86A3AA")
        d.polygon([(0, 58), (20, 48), (36, 37), (53, 48), (70, 43), (95, 58)], fill="#365D5B")
        d.polygon([(19, 51), (36, 39), (43, 47), (55, 51), (65, 54), (75, 58)], fill="#53756B")
        d.rectangle((0, 55, 227, 96), fill="#28667C")
        for y, color in [(62, "#2F7187"), (75, "#377F91"), (88, "#4A94A1")]:
            d.rectangle((0, y, 227, y + 7), fill=color)
        for y in range(60, 94, 7):
            for x in range((y * 5) % 23, 220, 31):
                width = 8 + (x + y) % 10
                d.rectangle((x, y, x + width, y + 1), fill="#6BA9B1" if y < 82 else "#A4CEC7")
                if y > 73:
                    d.rectangle((x + 4, y + 2, x + width - 3, y + 2), fill="#4C929E")
        d.rectangle((0, 96, 227, 134), fill="#B5A878")
        d.polygon([(0, 91), (36, 94), (68, 91), (104, 96), (144, 92), (183, 95), (227, 91),
                   (227, 99), (186, 101), (143, 98), (104, 102), (65, 98), (31, 101), (0, 98)], fill="#D0D4B5")
        d.polygon([(86, 134), (145, 134), (132, 110), (127, 98), (111, 96), (101, 110)], fill="#C8BA85")
        noise((0, 102, 228, 134), ["#C6B685", "#9E946C", "#BDAE7B"], 180, 21)
        for box in [(11, 108, 55, 124), (177, 105, 214, 119)]:
            d.ellipse(box, fill="#568B92")
            d.arc((box[0] + 3, box[1] + 2, box[2] - 4, box[3] - 3), 190, 340, fill="#A2C2B4")
        for x, y, w in [(3, 101, 24), (199, 98, 27), (51, 128, 19), (157, 124, 15)]:
            d.polygon([(x, y), (x + 5, y - 7), (x + w - 5, y - 5), (x + w, y + 2),
                       (x + 8, y + 5)], fill="#5D6E65")
            d.line((x + 5, y - 5, x + w - 7, y - 3), fill="#8B9782")
        for x, y in [(18, 130), (72, 113), (205, 127), (166, 105)]:
            grass(x, y, "#687E4E")
        for x, y in [(71, 126), (151, 115), (188, 131)]:
            d.arc((x, y - 2, x + 5, y + 2), 180, 360, fill="#E0CEA2")
            d.point((x + 2, y + 2), fill="#8C8061")
        d.line((24, 130, 48, 125), fill="#796847", width=3)
        d.line((35, 128, 31, 122), fill="#796847", width=2)
    elif route == "cave":
        d.rectangle((0, 0, 227, 134), fill="#201D30")
        noise((0, 0, 228, 100), ["#2B263D", "#352E47", "#41364F"], 260, 31)
        d.ellipse((27, -10, 202, 171), fill="#40374F")
        d.ellipse((43, 3, 187, 160), fill="#2B273C")
        d.ellipse((61, 19, 170, 154), fill="#151925")
        noise((12, 15, 67, 95), ["#4A3F58", "#312B43"], 85, 32)
        noise((169, 18, 218, 96), ["#4A3F58", "#312B43"], 85, 33)
        d.rectangle((0, 86, 227, 134), fill="#322E43")
        d.polygon([(88, 134), (145, 134), (132, 107), (128, 84), (115, 73),
                   (103, 87), (100, 111)], fill="#5B5365")
        d.polygon([(103, 134), (130, 134), (122, 106), (120, 88), (115, 82),
                   (111, 102), (112, 120)], fill="#6B6173")
        for x, y, height in [(3, 0, 36), (28, 0, 24), (54, 0, 18), (91, 0, 13),
                             (143, 0, 20), (174, 0, 29), (204, 0, 40)]:
            d.polygon([(x, y), (x + 17, y), (x + 9, y + height)], fill="#584C66")
            d.line((x + 4, y, x + 8, y + height - 6), fill="#716079")
        for x, y, height, color in [(24, 104, 26, "#83C7B7"), (48, 116, 17, "#A98CD0"),
                                    (183, 105, 25, "#8BCEBD"), (207, 117, 18, "#B69BDD"),
                                    (61, 91, 12, "#789FB0"), (165, 91, 13, "#8BA9BC")]:
            crystal(x, y, height, color)
        for box in [(7, 116, 65, 132), (166, 116, 220, 132)]:
            d.ellipse(box, fill="#263F4D")
            for x in range(box[0] + 8, box[2] - 7, 13):
                d.rectangle((x, box[1] + 5, x + 7, box[1] + 5), fill="#659497")
                d.rectangle((x + 3, box[1] + 9, x + 11, box[1] + 9), fill="#416D79")
        noise((73, 94, 157, 134), ["#827488", "#413A50", "#655B70"], 85, 34)
        for x, y in [(74, 48), (153, 38), (180, 73), (49, 67)]:
            d.point((x, y), fill="#91B5A9")
    else:
        raise ValueError(f"unknown route {route}")
    return image


def pixel_scene(route: str) -> Image.Image:
    return pixel_scene_source(route).resize((456, 270), Image.Resampling.NEAREST)
