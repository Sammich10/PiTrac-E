#!/usr/bin/python3

import cv2
import numpy as np
import os

CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))

# Board configuration
board_width = 10  # Number of squares in width
board_height = 7  # Number of squares in height
square_length = 0.020  # Square length in meters (20mm)
marker_length = 0.015  # Marker length in meters (15mm, 75% of square length)

# Letter paper is 8.5" x 11" = 215.9mm x 279.4mm
# At 300 DPI: 2550 x 3300 pixels
# We want some margin, so use effective area of ~200mm x 260mm

# Calculate pixel dimensions to maintain exact square size at 300 DPI
DPI = 300
pixels_per_meter = DPI / 0.0254  # 300 DPI = 11811 pixels/meter

# Calculate image size in pixels to match physical board dimensions
board_width_meters = board_width * square_length
board_height_meters = board_height * square_length
img_width_pixels = int(board_width_meters * pixels_per_meter)
img_height_pixels = int(board_height_meters * pixels_per_meter)

print(f"Board configuration:")
print(f"  Squares: {board_width} x {board_height}")
print(f"  Square size: {square_length*1000:.1f}mm")
print(f"  Marker size: {marker_length*1000:.1f}mm")
print(f"  Physical board size: {board_width_meters*1000:.1f}mm x {board_height_meters*1000:.1f}mm")
print(f"  Image size: {img_width_pixels} x {img_height_pixels} pixels at {DPI} DPI")
print(f"  This will fit on letter paper (215.9mm x 279.4mm) with margin")

# Create the board
dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_6X6_1000)
board = cv2.aruco.CharucoBoard((board_width, board_height), square_length, marker_length, dictionary)

# Generate the image at exact size for 300 DPI printing
img = board.generateImage((img_width_pixels, img_height_pixels))

# Add white border for better printing (10mm = ~118 pixels at 300 DPI)
border_pixels = int(0.010 * pixels_per_meter)  # 10mm border
img_with_border = cv2.copyMakeBorder(img, border_pixels, border_pixels, border_pixels, border_pixels, 
                                     cv2.BORDER_CONSTANT, value=255)

output_file = os.path.join(CURRENT_DIR, f"charuco_board_{board_width}x{board_height}_{int(square_length*1000)}mm.png")
cv2.imwrite(output_file, img_with_border)

print(f"\nBoard saved to: {output_file}")
print(f"\nPRINTING INSTRUCTIONS:")
print(f"1. Open the PNG file in an image viewer or Word")
print(f"2. Print at 100% scale (DO NOT scale to fit page)")
print(f"3. Printer settings: {DPI} DPI, no scaling")
print(f"4. After printing, verify square size with a ruler - should be {square_length*1000:.1f}mm")
print(f"5. Use setDimensions({board_width-1}, {board_height-1}) in your code (inner corners)")