#include <gtest/gtest.h>
#include "fake_shell.h"
#include "../scheduler_manager.h"

// ---- taskName -------------------------------------------------------------

TEST(SchedulerManager_TaskName, CorrectFormat) {
    EXPECT_EQ(SchedulerManager::taskName("20260427-Squad"), "SAGL_Unlock_20260427-Squad");
}

// ---- createTask -----------------------------------------------------------

TEST(SchedulerManager_CreateTask, SendsCorrectSchtasksCommand) {
    FakeShell shell;
    SchedulerManager sched(shell);

    sched.createTask("20260427-Squad",
                     "C:\\Tools\\sagl.exe",
                     "2026-05-02T13:30:00",
                     "05/02/2026",
                     "13:30");

    ASSERT_EQ(shell.executed.size(), 1u);
    const std::string& cmd = shell.executed[0];
    EXPECT_NE(cmd.find("schtasks /Create"),       std::string::npos);
    EXPECT_NE(cmd.find("SAGL_Unlock_20260427-Squad"), std::string::npos);
    EXPECT_NE(cmd.find("unlock --id 20260427-Squad --scheduled"), std::string::npos);
    EXPECT_NE(cmd.find("/SC ONCE"),               std::string::npos);
    EXPECT_NE(cmd.find("05/02/2026"),             std::string::npos);
    EXPECT_NE(cmd.find("13:30"),                  std::string::npos);
    EXPECT_NE(cmd.find("/RL HIGHEST"),            std::string::npos);
    EXPECT_NE(cmd.find("/Z"),                     std::string::npos);  // self-deletes after run
}

TEST(SchedulerManager_CreateTask, ReturnsEmptyOnFailure) {
    FakeShell shell;
    shell.executeReturnCode = 1;
    SchedulerManager sched(shell);

    EXPECT_TRUE(sched.createTask("lock1","C:\\sagl.exe","","01/01/2026","12:00").empty());
}

// ---- deleteTask -----------------------------------------------------------

TEST(SchedulerManager_DeleteTask, SendsCorrectCommand) {
    FakeShell shell;
    SchedulerManager sched(shell);

    sched.deleteTask("SAGL_Unlock_20260427-Squad");

    ASSERT_EQ(shell.executed.size(), 1u);
    EXPECT_NE(shell.executed[0].find("schtasks /Delete"),         std::string::npos);
    EXPECT_NE(shell.executed[0].find("SAGL_Unlock_20260427-Squad"), std::string::npos);
    EXPECT_NE(shell.executed[0].find("/F"),                       std::string::npos);
}

// ---- taskExists -----------------------------------------------------------

TEST(SchedulerManager_TaskExists, ReturnsTrueOnExitCode0) {
    FakeShell shell;
    shell.executeReturnCode = 0;
    SchedulerManager sched(shell);
    EXPECT_TRUE(sched.taskExists("SAGL_Unlock_20260427-Squad"));
}

TEST(SchedulerManager_TaskExists, ReturnsFalseOnNonZeroExitCode) {
    FakeShell shell;
    shell.executeReturnCode = 1;
    SchedulerManager sched(shell);
    EXPECT_FALSE(sched.taskExists("SAGL_Unlock_20260427-Squad"));
}

// ---- listSaglTasks --------------------------------------------------------

TEST(SchedulerManager_ListTasks, ParsesTasksFromCSVOutput) {
    FakeShell shell;
    // schtasks /Query /FO CSV /NH output format
    shell.captureResponses["schtasks /Query /FO CSV"] =
        "\"SAGL_Unlock_20260427-Squad\",\"N/A\",\"Ready\"\r\n"
        "\"Microsoft\\UpdateTask\",\"N/A\",\"Ready\"\r\n"
        "\"SAGL_Unlock_20260427-CS2\",\"N/A\",\"Ready\"\r\n";
    SchedulerManager sched(shell);

    auto tasks = sched.listSaglTasks();

    ASSERT_EQ(tasks.size(), 2u);
    EXPECT_EQ(tasks[0], "SAGL_Unlock_20260427-Squad");
    EXPECT_EQ(tasks[1], "SAGL_Unlock_20260427-CS2");
}

TEST(SchedulerManager_ListTasks, HandlesLeadingBackslashPrefix) {
    FakeShell shell;
    shell.captureResponses["schtasks /Query /FO CSV"] =
        "\"\\SAGL_Unlock_20260427-Squad\",\"N/A\",\"Ready\"\r\n";
    SchedulerManager sched(shell);

    auto tasks = sched.listSaglTasks();
    ASSERT_EQ(tasks.size(), 1u);
    EXPECT_EQ(tasks[0], "SAGL_Unlock_20260427-Squad");
}
