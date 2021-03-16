# Mandelbrot

Rus Lang:

Эта программа рисует [фрактал Мандельброта](https://en.wikipedia.org/wiki/Mandelbrot_set) с использованием фреймворка Qt. 
Скорость отрисовки увеличивается за счёт распараллеливания вычислений.
У пользователя есть возможность двигать изображение, менять масштаб, выбирать основной цвет фрактала и максимальное количество итераций обсчёта.

![Пример](https://github.com/Xagen37/Projects/blob/master/Mandelbrot/examples/start.png)

Чем больше число итераций, тем красивее и проработаннее получается кадр, но тем больше времени уходит на то, чтобы полностью просчитать кадр.
Чтобы пользователю не приходилось долго ждать каждый раз, как он захочет передвинуть кадр или зазумиться, программа сначала выводит черновое изображение.

![Превью](https://github.com/Xagen37/Projects/blob/master/Mandelbrot/examples/in_process.png)

Если в течение необходимого для обсчёта времени других запросов от пользователя не приходило, программа выведет кадр в нормальном качестве.

![Готовый кадр](https://github.com/Xagen37/Projects/blob/master/Mandelbrot/examples/ready.png)

Engl:

This program draws the [Mandelbrot fractal] (https://en.wikipedia.org/wiki/Mandelbrot_set) using the Qt framework. 
The rendering speed is increased by parallelizing the calculations.
The user has the ability to move the image, change the scale, select the main color of the fractal and the maximum number of iterations of the calculation.

![Example](https://github.com/Xagen37/Projects/blob/master/Mandelbrot/examples/start.png)

The greater the number of iterations, the more beautiful and detailed the frame is, but the more time it takes to fully calculate the frame.
So that the user does not have to wait for a long time every time he wants to move a frame or get smart, the program first outputs a draft image.

![Preview](https://github.com/Xagen37/Projects/blob/master/Mandelbrot/examples/in_process.png)

If no other requests have been received from the user during the time required for calculating, the program will display the frame in normal quality.

![Finished frame](https://github.com/Xagen37/Projects/blob/master/Mandelbrot/examples/ready.png)