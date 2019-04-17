#include<iostream>
#include<fstream>
#include<list>
#include<string>
#include<thread>
#include<mutex>
#include<queue>
#include<condition_variable>

//Дерево организовано про принципу n-арных деревьев
//В качестве значения узла 3 параметра
struct Employee {
	std::string name;
	std::string status;
	std::string date;
	std::list<Employee> slaves;
};

//Ищет запятую начиная с номера индекса n
std::string Found_chr(std::string s, int& n) {
	int i = n;
	std::string res;
	while (s[i] != ',' and i != s.size()) {
		res += s[i];
		i++;
	}
	n = ++i;
	return(res);
}


//Данные используемые в потоках
Employee emp;//Корень дерева
std::mutex mut;//Мьютекс
std::queue<Employee> data_queue;//Очередь, используемая для передачи информации между потоками
std::condition_variable data_cond;//Используем для извещения ожидающего потока



//Функция возвращает данные учетки, чтобы второй поток мог их вывести
Employee prepare_data(std::string s, Employee& emp) {
	int index = 0;
	std::string name_human = Found_chr(s, index);
	std::string name_status = Found_chr(s, index);
	std::string time_data = Found_chr(s, index);
	Employee new_emp;
	new_emp.name = name_human;
	new_emp.status = name_status;
	new_emp.date = time_data;

	/*В дереве всего 3 уровня:  Уровень 1 - Директор
							    Уровень 2 - Менеджера
							    Уровень 3 - Работник*/
	//Директор только один и он является корнем, за ним могут быть работники и менеджеры
	//Если есть хотя бы один менеджер среди учетных записей, то он и все остальные менеджеры будут на втором уровне, а работники на 3
	//Если менеджеров нет, то работники будут на втором уровне
	//Если нет директора, то работники и менеджеры гарантированно не займут уровень директора, то есть корень может быть пустым
	//Т.к. в условии нет точных критериев, то для простоты все рабочие добавляются либо в список к директору, если нет менеджеров
	//либо в список к последнему добавленному менеджеру
	if (name_status == "Director") {
		//Если это первая анкета из всех
		if (emp.status.size() == 0) {
			emp.name = name_human;
			emp.status = name_status;
			emp.date = time_data;
		}
		//Если анкета директора не была первой и до неё были другие
		//то делаем директора корнем дерева, т.к. нет никого выше директора
		//Предполагается, что в компании директор всего один, иначе не понятно какие отношения между директорами
		else {
			new_emp.slaves.push_back(emp);
		}
	}
	else if (name_status == "Manager") {
		if (emp.slaves.size() == 0) {
			emp.slaves.push_back(new_emp);
		}
		//Среди записей уже были менеджеры
		else if (emp.slaves.back().status == "Manager") {
			emp.slaves.push_back(new_emp);
		}
		//Этот случай означает, что среди учетных записей ещё не было менеджера, но были рабочие
		else {
			//Перетаскиваем всех рабочих в список менеджера и добавляем менеджера в список директора
			new_emp.slaves.splice(new_emp.slaves.begin(), emp.slaves);
			emp.slaves.push_back(new_emp);
		}
	}
	else {
		if (emp.slaves.size() == 0) {
			emp.slaves.push_back(new_emp);
		}
		//Если на втором уровне рабочие, то добавляем рабочего к ним
		else if (emp.slaves.back().status == "Worker") {
			emp.slaves.push_back(new_emp);
		}
		//Если на втором уровне менеджеры, то добавляем к последнему менеджеру
		else if (emp.slaves.back().status == "Manager") {
			emp.slaves.back().slaves.push_back(new_emp);
		}
	}
	return(new_emp);
}

//Проверяет, имеет ли смысл работать потокам
bool more_data_to_prepare(std::queue<std::string> data_q) {
	if (data_q.size() == 0) {
		return(false);
	}
	return(true);
}

//Первый поток
//Берет строковые данные из очереди, созданной главным потоком
//При помощи функции prepare_data добавляет данные в дерево и сохраняет их в параметр data
//data добавляется в очередь data_queue, которая используется для передачи данных между потоками
void data_preparation_thread(std::queue <std::string> queue) {
	while (more_data_to_prepare(queue)) {
		std::lock_guard<std::mutex> lk(mut);
		Employee const data = prepare_data(queue.front(),emp);
		queue.pop();
		data_queue.push(data);
		data_cond.notify_one();
	}
}

//Второй поток
//Получает преобразованные данные из очереди и выводит их
void data_processing_thread() {
	while (true) {
		std::unique_lock<std::mutex> lk(mut);
		data_cond.wait(lk, [] {return ! data_queue.empty(); });//проверяем очередь на пустотуc с помощью лямбда функции
		Employee data = data_queue.front();
		data_queue.pop();
		lk.unlock();
		//Выводит вновь полученные учетные данные 
		std::cout << "------------------------------------" << std::endl;
		std::cout << "New account: " << std::endl;
		std::cout << "\t    name: " << data.name << " | " << "status: " << data.status << " | " << "date: " << data.date << " | " << std::endl;
		if (!data_queue.size()) {
			break;
		}
	}
}

//Входные данные принимаются из файла
//Вид входных данных: Имя,должность,дата
int main() {
	std::ifstream in("data.txt");
	std::string s;
	std::queue<std::string> queue;
	if (!in.is_open()) {
		std::cout << "File is exist!\n";
		system("pause");
		return 0;
	}
	while (in >> s) {
		queue.push(s);
	}

	std::thread first(data_preparation_thread, ref(queue));
	std::thread second(data_processing_thread);
	first.join();
	second.join();
}