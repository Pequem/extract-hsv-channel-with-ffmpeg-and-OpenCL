#define HUE_DEGREE    512
#define MIN(a,b)      ((a) < (b) ? (a) : (b))
#define MAX(a,b)      ((a) > (b) ? (a) : (b))

#define MIN3(a,b,c)   MIN((a), MIN((b), (c)))
#define MAX3(a,b,c)   MAX((a), MAX((b), (c)))

void rgb2hsv(__global  unsigned char *d_rgb, __global  int *d_hsv, __global  unsigned  int *row_increment_, __global  unsigned int *bytes_per_pixel_, __global  unsigned int *res)
{
	int y = get_global_id(0) / res[0];
	int x = get_global_id(0) % res[0];
	const unsigned int y_offset = y * row_increment_[0];
	const unsigned int x_offset = x * bytes_per_pixel_[0];

	
    int r = d_rgb[y_offset + x_offset + 2];
    int g = d_rgb[y_offset + x_offset + 1];
    int b = d_rgb[y_offset + x_offset + 0];
    int m = MIN3(r, g, b);
    int M = MAX3(r, g, b);
    int delta = M - m;
	
    if(delta == 0) {
        /* Achromatic case (i.e. grayscale) */
        d_hsv[y_offset + x_offset + 0] = -1;          /* undefined */
        d_hsv[y_offset + x_offset + 1] = 0;
    } else {
        int h;

        if(r == M)
            h = ((g-b)*60*HUE_DEGREE) / delta;
        else if(g == M)
            h = (((b-r)*60*HUE_DEGREE) / delta) + (120*HUE_DEGREE);
        else /*if(b == M)*/
            h = (((r-g)*60*HUE_DEGREE) / delta) + (240*HUE_DEGREE);

        if(h < 0)
            h += 360*HUE_DEGREE;

        d_hsv[y_offset + x_offset + 0] = h;

        /* The constatnt 8 is tuned to statistically cause as little
         * tolerated mismatches as possible in RGB -> HSV -> RGB conversion.
         * (See the unit test at the bottom of this file.)
         */
        d_hsv[y_offset + x_offset + 1] = (256*delta-8) / M;
    }
    d_hsv[y_offset + x_offset + 2] = M;
}

void hsv2rgb(__global unsigned char *d_rgb, __global  int *d_hsv, __global  unsigned  int *row_increment_, __global  unsigned int *bytes_per_pixel_, __global  unsigned int *res)
{
	int y = get_global_id(0) / res[0];
	int x = get_global_id(0) % res[0];
	const unsigned int y_offset = y * row_increment_[0];
	const unsigned int x_offset = x * bytes_per_pixel_[0];

	char r, g, b;

    if(d_hsv[y_offset + x_offset + 1] == 0) {
        r = g = b = d_hsv[y_offset + x_offset + 2];
    } else {
        int h = d_hsv[y_offset + x_offset + 0];
        int s = d_hsv[y_offset + x_offset + 1];
        int v = d_hsv[y_offset + x_offset + 2];
        int i = h / (60*HUE_DEGREE);
        int p = (256*v - s*v) / 256;

        if(i & 1) {
            int q = (256*60*HUE_DEGREE*v - h*s*v + 60*HUE_DEGREE*s*v*i) / (256*60*HUE_DEGREE);
            switch(i) {
                case 1:   r = q; g = v; b = p; break;
                case 3:   r = p; g = q; b = v; break;
                case 5:   r = v; g = p; b = q; break;
            }
        } else {
            int t = (256*60*HUE_DEGREE*v + h*s*v - 60*HUE_DEGREE*s*v*(i+1)) / (256*60*HUE_DEGREE);
            switch(i) {
                case 0:   r = v; g = t; b = p; break;
                case 2:   r = p; g = v; b = t; break;
                case 4:   r = t; g = p; b = v; break;
            }
        }
    }

	d_rgb[y_offset + x_offset + 0] = b;
	d_rgb[y_offset + x_offset + 1] = g;
	d_rgb[y_offset + x_offset + 2] = r;
}

void kernel extracthsv(__global unsigned char *rgb, __global int* hsv, __global unsigned char *rgb_h, __global unsigned char *rgb_s, __global unsigned char *rgb_v, __global unsigned  int *row_increment_, __global unsigned int *bytes_per_pixel_, __global unsigned int *res) {
	int y = get_global_id(0) / res[0];
	int x = get_global_id(0) % res[0];
	const unsigned int y_offset = y * row_increment_[0];
	const unsigned int x_offset = x * bytes_per_pixel_[0];
	rgb2hsv(rgb, hsv, row_increment_, bytes_per_pixel_, res);
	int h, s, v;
	h = hsv[y_offset + x_offset + 0];
	s = hsv[y_offset + x_offset + 1];
	v = hsv[y_offset + x_offset + 2];
	hsv[y_offset + x_offset + 1] = 255;
	hsv[y_offset + x_offset + 2] = 255;
	hsv2rgb(rgb_h, hsv, row_increment_, bytes_per_pixel_, res);
	hsv[y_offset + x_offset + 0] = 0;
	hsv[y_offset + x_offset + 1] = s;
	hsv[y_offset + x_offset + 2] = 255;
	hsv2rgb(rgb_s, hsv, row_increment_, bytes_per_pixel_, res);
	hsv[y_offset + x_offset + 0] = 0;
	hsv[y_offset + x_offset + 1] = 0;
	hsv[y_offset + x_offset + 2] = v	;
	hsv2rgb(rgb_v, hsv, row_increment_, bytes_per_pixel_, res);
	
}