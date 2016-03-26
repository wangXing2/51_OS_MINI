#include "task.h"

/* 命令解释字段 */
code unsigned char cmd_buff[][100] = {
	"1.ps:\tdisplay the running tasks\r\n",
	"2.kill:\tkill the task you have selected\r\n\texample:\r\n\t\tkill 4 ---kill the 4th task\r\n",
	"3.new:\tcreate a task\r\n\texample:\r\n\t\tnew task2 ---create the task named 'task2'\r\n",
	"4.exit:\texit all this system\r\n",
	"\0"
};

/* 任务字串解析标志,解析完成标志置0,正在解析标志置1 */
xdata unsigned char parse_commond_flag = 0;

/* 等待命令解析 */
static void wait_parse(void)
{
	parse_commond_flag = 1;

	while(parse_commond_flag)
	{
		task_delay(2);	//阻塞，等待一个周期再来查询命令是否解析完毕
	}

}

/* 所有的消息处理模块 */
//'ps'
static void ps_msghandler(void *msg)
{
	xdata unsigned char i = 0;
	xdata unsigned char mod = 0;

/* 打印出来运行的任务概要 */
	printf("There are \"");
	uart_send_byte(task_running + 48);
	printf("\" tasks are running.");
/* 打印CPU从开机到现在运行的时间 */
	printf("\t Cpu has run \"");
	putchar(task_runtime / 10000000000+ 48);
	putchar(task_runtime % 1000000000 / 1000000000 + 48);
	putchar(task_runtime % 100000000 / 10000000 + 48);
	putchar(task_runtime % 10000000 / 1000000 + 48);
	putchar(task_runtime % 1000000 / 100000 + 48);
	putchar(task_runtime % 100000 / 10000 + 48);
	putchar(task_runtime % 10000 / 1000 + 48);
	putchar(task_runtime % 1000 / 100 + 48);
	putchar(task_runtime % 100 / 10 + 48);
	putchar(task_runtime % 10 + 48);
	printf("\" times\r\n");
	uart_send_str("pid\tpriority\tcpu_use\tname\r\n\r\n");
/* 遍历查询正在运行的任务并打印出运行详细情况来 */
	for(i = 0; i < TASK_MAX; i ++)
	{
		if(task_run_flag & (1 << i))
		{
			mod = (task_pcb_buf[i].run_time) * 100 / task_runtime;
			putchar(i + 48);
			putchar('\t');
			putchar(task_pcb_buf[i].priority / 100 + 48);
			putchar(task_pcb_buf[i].priority % 100 / 10 + 48);
			putchar(task_pcb_buf[i].priority % 10 + 48);
			printf("\t\t");
			putchar(mod / 10 + 48);
			putchar(mod % 10 + 48);
			putchar('\t');
			printf(task_pcb_buf[i].id);
			uart_send_str("\r\n");
		}
	}

//	uart_send_str("You do a 'test' commond\r\n");
}
//'kill	'pid''
static void kill_msghandler(void *msg)
{		
	if(cmd_split[1][0] != '2' && cmd_split[1][0] != '3' && cmd_split[1][0] != '0')
	{
		if(task_kill(cmd_split[1][0] - 48))
		{
			printf("The task don't exit\r\n");
		}
		else
		{
			uart_send_str("The task has been killed : pidnum = ");
			uart_send_byte(cmd_split[1][0]);
			uart_send_str("\r\n");
		}
	}
	else
	{
		uart_send_str("The task you select can't be killed\r\n");
	}
}
//'new 'pid''
static void new_msghandler(void *msg)
{	
	xdata unsigned char i;
	
	for(i = 0; i < TASK_MAX; i ++)
	{
		if(task_run_flag & (1 << i))
		{
			if(!os_strcmp(cmd_split[1], task_pcb_buf[i].id))
			{
				printf("The task has been exist\r\n");
				return;
			}
		}
	}
	
	if(!os_strcmp("task1", cmd_split[1]))
	{
		CREATE_TASK_RUNNING(2, task1, "task1");
		printf("The task has been created : task name = ");
		printf(cmd_split[1]);
		printf("\r\n");
	}
	else if(!os_strcmp("idle", cmd_split[1]))
	{
		CREATE_TASK_RUNNING(4, idle, "idle");
		printf("The task has been created : task name = ");
		printf(cmd_split[1]);
		printf("\r\n");
	}
	else if(!os_strcmp("task2", cmd_split[1]))
	{
		CREATE_TASK_RUNNING(3, task2, "task2");
		uart_send_str("The task has been created : task name = ");
		uart_send_str(cmd_split[1]);
		uart_send_str("\r\n");
	}
	else
	{
		uart_send_str("The task you select can't be created\r\n");
	}
}

/* 'help' ：获取帮助文件 */
static void help_msghandler(void *msg)
{
	unsigned char i = 0, j = 0;

	ENTER_CRITICAL;
	while(cmd_buff[i][0])
	{
		while(cmd_buff[i][j] != '\0')
		{
			putchar(cmd_buff[i][j]);
			j ++;
		}
		i ++;
		j = 0;
	}
	EXIT_CRITICAL;
}

/* 'exit' ：退出整个系统 */
static void exit_msghandler(void *msg)
{
	EA = 0;	//置零总中断寄存器，关闭整个系统,使单片机进入掉电模式更好
	TR1 = 0;
	ET1 = 0;
	while(1);
}

void task1(void)
{
	ENLED = 0;
	P2 = 0x06;

	while(1)
	{
		P07 = ~P07;			
		OS_delay(100);
	}		
}

void idle(void)
{
	
	while(1)
	{

	}
}

void task2(void)
{
	ENLED = 0;
	P2 = 0x06;
	
	while(1)
	{
		P06 = ~P06;
		OS_delay(50);
	}
}

/* 串口接收字符串进行回显，并把字符串放入一个缓冲区，之后发送命令消息给消息处理函数并解析执行 */
void task_tty0(void)
{
	static xdata unsigned int reve_buf_len = 0;	//初始化串口接收到的数据串长度为0

	printf("#");   	//开机打印出提示符

	while(1)
	{
		while(1)
		{				
			reve_buff[reve_buf_len] = uart_reve_byte();	//接收到一个字符并存取到接收缓冲区里面
			if(reve_buff[reve_buf_len] == 0) 			//如果没有接收到字串，继续查询
			{
				task_delay(2);							//延时两个任务周期，以便其他任务继续执行
				TF1 = 1;	//立马产生一个中断
			}
			else
				break;
		}

		if(reve_buf_len >= 254)
		{
			
			printf("\r\nToo much words\r\n");		//用户输入的字符数量过多
			printf("Please input your commond again and dont't more than 254\r\n#");		
//			clear_char_str(reve_buff, 256);		//清除整个数组，其实清除不清除并不影响，此处暂且注释掉
			reve_buf_len = 0;
		}	
		else
		{
			if(reve_buff[reve_buf_len] == '\b')
			{
				if(reve_buf_len > 0)
				{
					reve_buf_len --;
					printf("\b \b");
				}
			}
			else if(reve_buff[reve_buf_len] == '\r')	  	//接收到换行符
			{
				reve_buff[reve_buf_len] = '\0';		//将当前的换行字符替换为字符串结束符用来比较
				DESUSPEND_TASK(TASK_MSGHANDLER);	//消息处理函数解挂起
				wait_parse();						//等待命令解析完成
//				clear_char_str(reve_buff, 256);		//清除整个数组，其实清除不清除并不影响，此处暂且注释掉
				reve_buf_len = 0;					//字串的索引置0以便重新接受输入
			}
			else
			{
				uart_send_byte(reve_buff[reve_buf_len]);	//将刚刚接收到的一个字节并且发送给串口软件显示
				reve_buf_len ++;							//字串索引值加1
			}
			
		}
	}	
}

/* 消息处理函数，对串口接收到的命令进行相应的处理 */
void task_msghandler(void)
{	
	while(1)
	{		
		uart_send_str("\r\n");
		
		split_str(reve_buff, cmd_split);
		
		if(!os_strcmp("kill", cmd_split[0]))
		{
			kill_msghandler(0);
		}
		else if(!os_strcmp("new", cmd_split[0]))
		{
			new_msghandler(0);
		}
		else if(!os_strcmp("ps", cmd_split[0]))
		{
			ps_msghandler(0);
		}
		else if(!os_strcmp("help", cmd_split[0]))
		{
			help_msghandler(0);
		}
		else if(!os_strcmp("exit", cmd_split[0]))
		{
			exit_msghandler(0);
		}
		else if(os_strcmp("", cmd_split[0]))
		{
			uart_send_str(cmd_split[0]);
			uart_send_str(": undefined commond\r\n");
		}
				
		uart_send_str("#");	//发送提示符
		parse_commond_flag = 0;	//任务解析完毕
		SUSPEND_TASK(TASK_MSGHANDLER);	//自挂起，应该先挂起再等待
		TF1 = 1;			//立刻产生一个中断，避免多次执行到任务	
	}
}

//void timer0_irq(void) interrupt 1
//{
//	lq_disp(0x123456);
//}