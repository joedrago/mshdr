#ifndef IMAGE_H
#define IMAGE_H

class Image
{
public:
    Image();
    ~Image();

    bool load(const char * filename);

    unsigned int width() { return width_; }
    unsigned int height() { return height_; }
    unsigned int depth() { return depth_; }
    unsigned char * pixels() { return pixels_; }

    unsigned int bpp()
    {
        if (depth_ > 8)
            return 8;
        return 4;
    }

    unsigned int bytes()
    {
        return width_ * height_ * bpp();
    }

protected:
    unsigned int width_;
    unsigned int height_;
    unsigned int depth_;
    unsigned char * pixels_;
};

#endif // ifndef IMAGE_H
