#!/usr/bin/env python3
import os
import argparse
import cv2
import numpy as np
import glob


# The distortion coefficients are used to model the radial and tangential
# distortion present in the camera lens.
# The radial distortion is primarily caused by the lens shape and results in
# curvature effect in the image.
# - k1 and k2 are the radial distortion coefficients.
#   - k1 represents the primary radial distortion term.
#   - k2 represents the secondary radial distortion term.
#   - These values are typically negative for barrel distortion and positive
#       for pincushion distortion.
# - p1 and p2 are the tangential distortion coefficients.
#   - They account for the slight off-centeredness of the lens components.
#   - These values are usually close to 0 unless there are lens misalignments.
# - k3 is an additional radial distortion coefficient, typically kept at 0
#        unless higher-order distortion is present.
#   - Higher-order distortion effects are generally less common and often not
#        needed to be considered.
# The calibration process involves capturing images of known calibration
# patterns (e.g., chessboard) from different
# angles and distances, and then iteratively adjusting the distortion
# coefficients to minimize the difference between
# the observed and expected positions of calibration points.
def get_dist_coeffs(images):
    # Find chessboard corners
    # Set the parameters for finding subpixel corners. The stopping criterion
    # is either a maximum of 30 iterations or an accuracy of 0.001
    criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)

    # Chessboard pattern dimensions
    w = 9   # 10 - 1
    h = 9   # 10 - 1

    # Chessboard points in the world coordinate system,
    # e.g., (0,0,0), (1,0,0), (2,0,0), ..., (8,8,0).
    # Drop the Z coordinate to get a 2D matrix.
    objp = np.zeros((w*h, 3), np.float32)
    objp[:, :2] = np.mgrid[0:w, 0:h].T.reshape(-1, 2)
    # 26.2 mm, which is the physical length of the chessboard square
    objp = objp * 26.2

    # Storage for world coordinates and image coordinates
    # of the chessboard corners
    objpoints = []  # 3D points in the world coordinate system
    imgpoints = []  # 2D points in the image plane

    for fname in images:
        img = cv2.imread(fname)
        # Get the center point of the image
        # Get the width and height of the image
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        u, v = img.shape[:2]
        # Find the chessboard corners
        ret, corners = cv2.findChessboardCorners(gray, (w, h), None)
        print(f"Image file name: {fname}, Return code: {ret}")
        # If enough corners are found, store them
        if ret:
            print(f"Corners shape: {corners.shape}")
            # Refine the corner locations to subpixel accuracy
            cv2.cornerSubPix(gray, corners, (11, 11), (-1, -1), criteria)
            # Add to the list of 3D points and 2D points
            objpoints.append(objp)
            imgpoints.append(corners)
            # Draw and display the corners on the image
            cv2.drawChessboardCorners(img, (w, h), corners, ret)
            if os.environ.get('DISPLAY'):
                cv2.namedWindow('findCorners', cv2.WINDOW_NORMAL)
                cv2.resizeWindow('findCorners', 640, 480)
                cv2.imshow('findCorners', img)
                cv2.waitKey(200)
        else:
            print(f"No corners found for image file: {fname}")
            print("Please check the image quality and ensure the chessboard"
                  " is fully visible and clear.")
            exit(1)

    cv2.destroyAllWindows()
    ret, mtx, dist, rvecs, tvecs = \
        cv2.calibrateCamera(objpoints, imgpoints, gray.shape[::-1], None, None)

    # Print the calculated parameters
    print("Intrinsic matrix (mtx):\n", mtx)
    # Distortion coefficients = (k1, k2, p1, p2, k3)
    print("Distortion coefficients (dist):\n", dist)
    # Rotation vectors (extrinsic parameters)
    print("Rotation vectors (rvecs):\n", rvecs)
    # Translation vectors (extrinsic parameters)
    print("Translation vectors (tvecs):\n", tvecs)
    newcameramtx, roi = cv2.getOptimalNewCameraMatrix(
        mtx, dist, (u, v), 0, (u, v))
    print('New camera matrix (newcameramtx):\n', newcameramtx)
    return dist, newcameramtx


def undistort(frame, dist, mtx):
    k = np.array(mtx)
    d = np.array(dist)
    h, w = frame.shape[:2]
    mapx, mapy = cv2.initUndistortRectifyMap(k, d, None, k, (w, h), 5)
    return cv2.remap(frame, mapx, mapy, cv2.INTER_LINEAR), mapx, mapy


def main():
    if not os.environ.get('DISPLAY'):
        print("No graphical environment detected. Skipping display of images.")

    parser = argparse.ArgumentParser(
        description='Gdc calibration and image undistortion.')
    parser.add_argument(
        '-i',
        '--input_images_dir',
        type=str, default='./chessboard',
        help='Directory containing the chessboard images.')
    parser.add_argument(
        '-t',
        '--test_image',
        type=str,
        default='./chessboard/vlcsnap-2024-05-06-09h53m19s733.jpg',
        help='File path of the image to be undistorted.')
    parser.add_argument(
        '-o',
        '--output_file',
        type=str, default='custom_config.txt',
        help='File path for the output configuration.')

    args = parser.parse_args()

    print(f'Input images directory: {args.input_images_dir}')
    print(f'Test image file: {args.test_image}')
    print(f'Output file: {args.output_file}')

    # Validate the image directory
    images = glob.glob(f'{args.input_images_dir}/*.png') \
        + glob.glob(f'{args.input_images_dir}/*.jpg')
    if not images:
        print(f"No images found in the directory {args.input_images_dir}")
        return

    # Validate the image file
    if not cv2.imread(args.test_image) is not None:
        print(f"Image file {args.test_image} not found or could not be read.")
        return

    # Load all checkerboard images, support both PNG and JPG formats
    # Directory containing a dozen checkerboard images
    images = glob.glob(f'{args.input_images_dir}/*.png') \
        + glob.glob(f'{args.input_images_dir}/*.jpg')
    # Compute distortion coefficients and intrinsic matrix
    dist, mtx = get_dist_coeffs(images)
    print("Validation of distortion correction")
    img = cv2.imread(args.test_image)
    dst_img, mapx, mapy = undistort(img, dist, mtx)

    print("Saving mapx and mapy to '{}'".format(args.output_file))
    # Output custom configuration to a text file
    h, w = img.shape[:2]
    output_str = ""

    output_str += str(1) + "\n"
    output_str += str(50) + " " + str(50) + "\n"
    output_str += str(h) + " " + str(w) + "\n"
    output_str += str(h // 2 - 1) + " " + str(w // 2 - 1) + "\n"

    # Collect remapped coordinates
    for i in range(0, h):
        for j in range(0, w):
            if mapy[i, j] < 0:
                mapy[i, j] = 0
            if mapx[i, j] < 0:
                mapx[i, j] = 0
            output_str += str(mapy[i, j]) + ":" + str(mapx[i, j]) + " "
        output_str += "\n"

    # Write collected data to the file
    with open(args.output_file, 'w') as f:
        f.write(output_str)

    # Display the original image if a graphical environment is available
    if os.environ.get('DISPLAY'):
        # Concatenate the two images horizontally into one large image
        combined_img = np.hstack((img, dst_img))

        # Create a window and set size limits
        cv2.namedWindow("Comparison", cv2.WINDOW_NORMAL)
        cv2.resizeWindow("Comparison", 960, 320)

        # Display the combined image
        cv2.imshow("Comparison", combined_img)
        cv2.waitKey(3000)
        cv2.destroyAllWindows()
    else :
        print("No graphical environment detected. "
              "The output image has been saved to '{}'."
              .format(args.output_file))


if __name__ == '__main__':
    main()
