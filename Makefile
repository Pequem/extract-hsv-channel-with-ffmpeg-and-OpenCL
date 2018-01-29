all:
	g++ *.cpp -w -o prog -L lib/ -I include -std=c++11 -Wall -l OpenCL
	echo "Para executar coloque o nome do video a ser codificado como parametro. Ex:./prog acao.mp4"
