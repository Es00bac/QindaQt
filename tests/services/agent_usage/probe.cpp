// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/agent_usage/agent_usage_collector.h>
#include <QCoreApplication>
#include <QTimer>
#include <QTextStream>
using namespace QindaQt::Services::AgentUsage;
int main(int argc,char **argv)
{
    QCoreApplication app(argc,argv);
    AgentUsageCollector source("/nonexistent-agent-usage-probe","/usr/bin/codex",[] {return QDateTime::currentDateTimeUtc();});
    QObject::connect(&source,&AgentUsageSource::snapshotChanged,&app,[&] {
        const auto u=source.snapshot().first();
        if(u.detail=="No usage report"||u.detail=="Collecting usage metadata") return;
        QTextStream(stdout)<<"state="<<static_cast<int>(u.state)<<" quotaWindows="<<u.quotaWindows.size()
            <<" tokenMetrics="<<(u.inputTokens.has_value()+u.outputTokens.has_value()+u.totalTokens.has_value())
            <<" costMetrics="<<u.reportedCostUsd.has_value()<<"\n";
        app.quit();
    });
    QTimer::singleShot(8000,&app,[&] {app.exit(2);});
    source.refresh();return app.exec();
}
