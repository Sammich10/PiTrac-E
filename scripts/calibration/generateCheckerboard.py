#!/usr/bin/python3

import cv2
import numpy as np
import os

CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))

# Board configuration
board_width = 9  # Number of inner corners in width
board_height = 6  # Number of inner corners in height
square_length = 0.025  # Square length in meters (25mm)

# Letter paper is 8.5" x 11" = 215.9mm x 279.4mm
# At 300 DPI: 2550 x 3300 pixels
# We want some margin, so use effective area of ~200mm x 260mm

# Calculate pixel dimensions to maintain exact square size at 300 DPI
DPI = 300
pixels_per_meter = DPI / 0.0254  # 300 DPI = 11811 pixels/meter

# Checkerboard has (inner_corners + 1) squares per side
num_squares_width = board_width + 1
num_squares_height = board_height + 1

# Calculate square size in pixels
square_size_pixels = int(square_length * pixels_per_meter)

# Calculate total board size
img_width_pixels = num_squares_width * square_size_pixels
img_height_pixels = num_squares_height * square_size_pixels

board_width_meters = num_squares_width * square_length
board_height_meters = num_squares_height * square_length

print(f"Checkerboard configuration:")
print(f"  Inner corners: {board_width} x {board_height}")
print(f"  Squares: {num_squares_width} x {num_squares_height}")
print(f"  Square size: {square_length*1000:.1f}mm ({square_size_pixels} pixels)")
print(f"  Physical board size: {board_width_meters*1000:.1f}mm x {board_height_meters*1000:.1f}mm")
print(f"  Image size: {img_width_pixels} x {img_height_pixels} pixels at {DPI} DPI")
print(f"  This will fit on letter paper (215.9mm x 279.4mm) with margin")

# Create checkerboard pattern
img = np.zeros((img_height_pixels, img_width_pixels), dtype=np.uint8)

# Fill in the checkerboard pattern
for row in range(num_squares_height):
    for col in range(num_squares_width):
        # Alternate black (0) and white (255)
        if (row + col) % 2 == 0:
            y_start = row * square_size_pixels
            y_end = (row + 1) * square_size_pixels
            x_start = col * square_size_pixels
            x_end = (col + 1) * square_size_pixels
            img[y_start:y_end, x_start:x_end] = 255

# Add white border for better printing (10mm = ~118 pixels at 300 DPI)
border_pixels = int(0.010 * pixels_per_meter)  # 10mm border
img_with_border = cv2.copyMakeBorder(img, border_pixels, border_pixels, border_pixels, border_pixels, 
                                     cv2.BORDER_CONSTANT, value=255)

output_file = os.path.join(CURRENT_DIR, f"checkerboard_{board_width}x{board_height}_{int(square_length*1000)}mm.png")
cv2.imwrite(output_file, img_with_border)

print(f"\nCheckerboard saved to: {output_file}")
print(f"\nPRINTING INSTRUCTIONS:")
print(f"1. Open the PNG file in an image viewer or Word")
print(f"2. Print at 100% scale (DO NOT scale to fit page)")
print(f"3. Printer settings: {DPI} DPI, no scaling")
print(f"4. After printing, verify square size with a ruler - should be {square_length*1000:.1f}mm")
print(f"5. Use setDimensions({board_width}, {board_height}) in your code (inner corners)")
print(f"\nNOTE: Inner corners are the points where 4 squares meet (not the outer edges)")
print(f"      A {num_squares_width}x{num_squares_height} checkerboard has {board_width}x{board_height} inner corners")
