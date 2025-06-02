% 读取data.log文件
data = load('data.log');

% 绘制数据
figure;
plot(data, 'o-', 'LineWidth', 1.5);
grid on;
xlabel('数据点索引');
ylabel('数据值');
title('data.log文件数据可视化');

% 显示一些基本统计信息
disp(['数据最小值: ', num2str(min(data))]);
disp(['数据最大值: ', num2str(max(data))]);
disp(['数据平均值: ', num2str(mean(data))]);
disp(['数据标准差: ', num2str(std(data))]);
