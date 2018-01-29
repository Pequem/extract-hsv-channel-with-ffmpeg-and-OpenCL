#include "bitmap.h"
#include <iostream>
#include <string>
#include "CL/cl.hpp"

using namespace std;
int get_duration(string filename) {
	filename = "ffmpeg -i " + filename + " > info.txt";
system(filename.c_str());
ifstream f("info.txt");
string s;
for (;;) {
	getline(f, s);
	if (s.find("Duration:") != string::npos) {
		break;
	}
}
cout << s;
return 0;
}

typedef struct gpu {
	cl::Context context;
	cl::Device default_device;
	cl::Program program;
}GPU;

void prepareGPU(GPU &gpu) {
	vector<cl::Platform> all_platform;
	cl::Platform::get(&all_platform);
	if (all_platform.size() == 0) {
		cout << "Hardware nao possui support ao OpenCL" << endl;
		exit(1);
	}
	int pc = 0;
	cout << "Selecione a plataforma:" << endl;
	for (auto p : all_platform) {
		cout << pc << " - " << p.getInfo<CL_PLATFORM_NAME>() << endl;
		pc++;
	}
	cin >> pc;
	cl::Platform default_platform = all_platform[pc];
	std::cout << "Usando plataforma: " << default_platform.getInfo<CL_PLATFORM_NAME>() << "\n";

	vector<cl::Device> all_devices;
	default_platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);
	if (all_devices.size() == 0) {
		cout << "Sem dispositivo" << endl;
		exit(1);
	}
	pc = 0;
	cout << "Selecione o dispositivo:" << endl;
	for (auto p : all_devices) {
		cout << pc << " - " << p.getInfo<CL_DEVICE_NAME>() << endl;
		pc++;
	}
	cin >> pc;
	cl::Device default_device = all_devices[pc];
	std::cout << "Usando dispositivo: " << default_device.getInfo<CL_DEVICE_NAME>() << "\n";

	cl::Context context({ default_device });

	cl::Program::Sources sources;
	string s("");
	string aux;
	ifstream cl("HSVtoRGB.cl");
	while (getline(cl, aux)) {
		s += aux;
		s += "\n";
	}

	sources.push_back({ s.c_str(), s.length() });
	cl::Program program(context, sources);
	auto err = program.build({ default_device });
	if (err != CL_SUCCESS) {
		std::cout << " Error building: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(default_device) << "\n";
		exit(0);
	}
	gpu.context = context;
	gpu.default_device = default_device;
	gpu.program = program;
}

int convert(long frame, GPU &gpu)
{

	for (int i = 1, j = frame;; i++, j++) {


		bitmap_image image("frames/source/" + std::to_string(i) + ".bmp");
		if (!image) {
			return i - 1;
		}
		unsigned int height, width;
		height = image.height();
		width = image.width();
		bitmap_image image_h(width, height), image_s(width, height), image_v(width, height);

		cl::Buffer rgb(gpu.context, CL_MEM_READ_WRITE, sizeof(unsigned char) * image.data_.size());
		cl::Buffer hsv_(gpu.context, CL_MEM_READ_WRITE, sizeof(int) * image.data_.size());
		cl::Buffer rgb_h(gpu.context, CL_MEM_READ_WRITE, sizeof(unsigned char) * image.data_.size());
		cl::Buffer rgb_s(gpu.context, CL_MEM_READ_WRITE, sizeof(unsigned char) * image.data_.size());
		cl::Buffer rgb_v(gpu.context, CL_MEM_READ_WRITE, sizeof(unsigned char) * image.data_.size());
		cl::Buffer row(gpu.context, CL_MEM_READ_WRITE, sizeof(unsigned int));
		cl::Buffer byte(gpu.context, CL_MEM_READ_WRITE, sizeof(unsigned int));
		cl::Buffer res(gpu.context, CL_MEM_READ_WRITE, sizeof(unsigned int));
		cl::Kernel kernel(gpu.program, "extracthsv");
		kernel.setArg(0, rgb);
		kernel.setArg(1, hsv_);
		kernel.setArg(2, rgb_h);
		kernel.setArg(3, rgb_s);
		kernel.setArg(4, rgb_v);
		kernel.setArg(5, row);
		kernel.setArg(6, byte);
		kernel.setArg(7, res);
		cl::CommandQueue queue(gpu.context, gpu.default_device);
		cl::Event ev;
		queue.enqueueWriteBuffer(rgb, CL_TRUE, 0, sizeof(unsigned char) * image.data_.size(), image.data_.data());
		queue.enqueueWriteBuffer(row, CL_TRUE, 0, sizeof(unsigned int), &image.row_increment_);
		queue.enqueueWriteBuffer(byte, CL_TRUE, 0, sizeof(unsigned int), &image.bytes_per_pixel_);
		queue.enqueueWriteBuffer(res, CL_TRUE, 0, sizeof(unsigned int), &width);
		if (queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(width*height)) != CL_SUCCESS) {
			exit(1);
		}
		queue.finish();

		queue.enqueueReadBuffer(rgb_h, CL_TRUE, 0, sizeof(unsigned char)*image.data_.size(), image_h.data_.data());
		queue.enqueueReadBuffer(rgb_s, CL_TRUE, 0, sizeof(unsigned char)*image.data_.size(), image_s.data_.data());
		queue.enqueueReadBuffer(rgb_v, CL_TRUE, 0, sizeof(unsigned char)*image.data_.size(), image_v.data_.data());

		image_h.save_image("frames/h/"+std::to_string(j)+".bmp");
		image_s.save_image("frames/s/" + std::to_string(j) + ".bmp");
		image_v.save_image("frames/v/" + std::to_string(j) + ".bmp");
	}
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		cout << "nao ha parametros de entrada, digite o arquivo a ser processado (.mp4)" << endl;
		exit(1);
	}
	string comando;
	string filename;
	filename = argv[1];
	long frames, atualframes = 0;
	GPU gpu;
	prepareGPU(gpu);
	int videos = 0;
	for (int i = 0;; i++) {
		printf("%i\n", i);
		comando = "ffmpeg -i " + filename + " -ss " + to_string(i) + " -t 1 frames/source/%d.bmp";
		system(comando.c_str());
		frames = convert(1, gpu);
		if (frames == 0) {
			break;
		}
		//atualframes += frames;
		system(("ffmpeg -r 60 -f image2 -s 1920x1080 -i frames/h/%d.bmp -vcodec libx264 -crf 25 -pix_fmt yuv420p videos/h/"+to_string(videos+1)+".mp4").c_str());
		system(("ffmpeg -r 60 -f image2 -s 1920x1080 -i frames/s/%d.bmp -vcodec libx264 -crf 25 -pix_fmt yuv420p videos/s/" +to_string(videos+1) +".mp4").c_str());
		system(("ffmpeg -r 60 -f image2 -s 1920x1080 -i frames/v/%d.bmp -vcodec libx264 -crf 25 -pix_fmt yuv420p videos/v/" +to_string(videos+1) +".mp4").c_str());
		videos++;
		for (int j = 1; j <= frames; j++) {
			remove(("frames/source/"+std::to_string(j)+".bmp").c_str());
			remove(("frames/h/" + std::to_string(j) + ".bmp").c_str());
			remove(("frames/s/" + std::to_string(j) + ".bmp").c_str());
			remove(("frames/v/" + std::to_string(j) + ".bmp").c_str());
		}
	}
		ofstream fh("listh.txt");
		ofstream fs("lists.txt");
		ofstream fv("listv.txt");
	for (int i = 1; i <= videos; i++) {
		fh << "file 'videos/h/";
		fh << to_string(i);
		fh << ".mp4'";
		fh << endl;
		fs << "file 'videos/s/";
		fs << to_string(i);
		fs << ".mp4'";
		fs << endl;
		fv << "file 'videos/v/";
		fv << to_string(i);
		fv << ".mp4'";
		fv << endl;

	}
	fh.close();
	fv.close();
	fs.close();
	system("ffmpeg -f concat -i listh.txt -c copy videos/h.mp4");
	system("ffmpeg -f concat -i listv.txt -c copy videos/v.mp4");
	system("ffmpeg -f concat -i lists.txt -c copy videos/s.mp4");
	remove("listh.txt");
	remove("lists.txt");
	remove("listv.txt");
	for (int i = 1; i <= videos; i++) {
		remove(("videos/h/"+to_string(i)+".mp4").c_str());
		remove(("videos/s/" + to_string(i) + ".mp4").c_str());
		remove(("videos/v/" + to_string(i) + ".mp4").c_str());
	}
}

