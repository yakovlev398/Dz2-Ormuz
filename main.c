#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "lodepng.h"

// принимаем на вход: имя файла, указатели на int для хранения прочитанной ширины и высоты картинки
// возвращаем указатель на выделенную память для хранения картинки
// Если память выделить не смогли, отдаем нулевой указатель и пишем сообщение об ошибке
unsigned char* load_png(const char* filename, unsigned int* width, unsigned int* height)
{
    unsigned char* image = NULL;
    int error = lodepng_decode32_file(&image, width, height, filename);
    if(error != 0) {
        printf("error %u: %s\n", error, lodepng_error_text(error));
    }
    return (image);
}

// принимаем на вход: имя файла для записи, указатель на массив пикселей,  ширину и высоту картинки
// Если преобразовать массив в картинку или сохранить не смогли,  пишем сообщение об ошибке
void write_png(const char* filename, const unsigned char* image, unsigned width, unsigned height)
{
  unsigned char* png;
  size_t pngsize;
  int error = lodepng_encode32(&png, &pngsize, image, width, height);
  if(error == 0) {
      lodepng_save_file(png, pngsize, filename);
  } else {
    printf("error %u: %s\n", error, lodepng_error_text(error));
  }
  free(png);
}

// вариант огрубления серого цвета в ЧБ
void contrast(unsigned char *col, int bw_size)
{
    int i;
    for (i=0;i<bw_size;i++)
    {
        if (col[i] <= 75)
            col[i] = 0;
        if (col[i]>190)
            col[i] = 255;
    }
    return;
}

// Гауссово размыттие
void Gauss_blur(unsigned char *col, unsigned char *blr_pic, int width, int height)
{
    int i, j;
    for(i=1; i < height-1; i++)
        for(j=1; j < width-1; j++)
        {
            blr_pic[width*i+j] = 0.084*col[width*i+j] + 0.084*col[width*(i+1)+j] + 0.084*col[width*(i-1)+j];
            blr_pic[width*i+j] = blr_pic[width*i+j] + 0.084*col[width*i+(j+1)] + 0.084*col[width*i+(j-1)];
            blr_pic[width*i+j] = blr_pic[width*i+j] + 0.063*col[width*(i+1)+(j+1)] + 0.063*col[width*(i+1)+(j-1)];
            blr_pic[width*i+j] = blr_pic[width*i+j] + 0.063*col[width*(i-1)+(j+1)] + 0.063*col[width*(i-1)+(j-1)];
        }
   return;
}

int work_area[10][4]={
                        {549, 303, 705, 47},
                        {626, 644, 1097, 310},
                        {563, 601, 627, 442},
                        {534, 558, 563, 449},
                        {566, 433, 624, 308},
                        {510, 405, 564, 338},
                        {503, 335, 534, 328},
                        {551, 332, 563, 321},
                        {549, 321, 563, 303},
                        {502, 315, 549, 301}
                        };

//проверка: точка входит в какую-либо рабочую зону
int in_any_work_area(int x, int y)
{
    for (int i=0; i<10; i++)
    {
        if (x>=work_area[i][0] && x<=work_area[i][2] &&
            y>=work_area[i][3] && y<=work_area[i][1])
            return 1;
    }
    return 0;
}

// создание матрицы смежности (adjacency matrix)
 void create_AM(int* AM, int bw_size, unsigned char* finish)
 {
     for (int i=0;i<bw_size;i++)
         if (0.299*finish[4*i] + 0.587*finish[4*i+1] + 0.114*finish[4*i+2]>17)
             AM[i]=1;
         else AM[i]=0;
 }

void dfs(int x, int y, int* AM, int width, int height, int* visited)
{
    if (x<0 || x>=width || y<0 || y>=height) return; //вышли за границы
    if (visited[y*width+x]) return; // уже посещен
    if (AM[(y * width + x)]==0) return; // не корабль и не его часть
    visited[y*width+x] = 1; //теперь посещен
    // Обход соседних
    dfs(x+1,y,AM,width,height,visited);
    dfs(x-1,y,AM,width,height,visited);
    dfs(x,y+1,AM,width,height,visited);
    dfs(x,y-1,AM,width,height,visited);
}

int count_ship(int* AM, int width, int height)
{
    int* visited=(int*)calloc(width*height,sizeof(int));
    int count=0;
    for (int x=0;x<width;x++)
        for (int y=0;y<height;y++)
            if (visited[y*width+x]==0 && AM[y*width+x]==1)
            {
                dfs(x,y,AM,width,height,visited);
                count++;
            }
    free(visited);
    return count;
}

int main()
{
    const char* filename = "skull.png";
    unsigned int width, height;
    int size;
    int bw_size;

    // Прочитали картинку
    unsigned char* picture = load_png("skull.png", &width, &height);
    if (picture == NULL)
    {
        printf("Problem reading picture from the file %s. Error.\n", filename);
        return -1;
    }

    size = width * height * 4;
    bw_size = width * height;

    unsigned char* bw_pic = (unsigned char*)malloc(bw_size*sizeof(unsigned char));
    unsigned char* blr_pic = (unsigned char*)malloc(bw_size*sizeof(unsigned char));
    unsigned char* finish = (unsigned char*)malloc(size*sizeof(unsigned char));

    // формула для расчета яркости пикселя
    for (int i=0;i<bw_size;i++)
        bw_pic[i]=0.299*picture[4*i] + 0.587*picture[4*i+1] + 0.114*picture[4*i+2];

    //делаем все пискеси не из рабочих областей черными
    for (int y=0;y<height;y++)
        for (int x=0;x<width;x++)
            if (!in_any_work_area(x,y))
                bw_pic[y*width+x]=0;

    // контраст
    contrast(bw_pic, bw_size);
    for(int i=0; i<bw_size; i++)
    {
        finish[4*i] = finish[4*i+1] = finish[4*i+2] = bw_pic[i];
        finish[4*i+3] = 255;
    }
    // картинка после контраста
    write_png("contrast.png", finish, width, height);

    // Гауссово размытие
    Gauss_blur(bw_pic, blr_pic, width, height);
    for(int i=0; i<bw_size; i++)
    {
        finish[4*i] = finish[4*i+1] = finish[4*i+2] = blr_pic[i];
        finish[4*i+3] = 255;
    }
    // картинка после Гауссова размытия
    write_png("gauss.png", finish, width, height);

    int* AM=(int*)malloc(bw_size*sizeof(int));
    create_AM(AM, bw_size, finish);

    printf("Numbers of ships: %d\n", count_ship(AM,width,height));

    //результат
    write_png("picture_out.png", finish, width, height);

    // не забыли почистить память!
    free(bw_pic);
    free(blr_pic);
    free(finish);
    free(picture);
    free(AM);
    return 0;
}