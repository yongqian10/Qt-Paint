#include "historyworker.h"
#include <QObject>
#include <QVector>
#include <QSignalBlocker>
#include "../packages/basicpackage.h"
#include "../../common/commands/drawcommand.h"
#include "../../common/builders/drawcommandfactory.h"

void HistoryWorker::track(DrawHistory& history)
{
    m_pHistory = &history;
    connect(m_pHistory, &DrawHistory::historyChanged,
                this, &HistoryWorker::onHistoryChanged);
}

void HistoryWorker::addClient(QTcpSocket* socket)
{
    if (socket) {
        m_clients.insert(socket);
    }
}

void HistoryWorker::removeClient(QTcpSocket* socket)
{
   m_clients.erase(socket);
}

bool HistoryWorker::isValidHash(uint64_t hash) const
{
    return hash == m_historyHash.totalHash();
}

int HistoryWorker::getHashesDiffPosition(const QVector<quint64>& newHashes) const
{
    const auto& currentHashes = m_historyHash.commandHashes();

    int currentSize {currentHashes.size()};
    int newSize {newHashes.size()};
    int minSize {std::min(currentSize, newSize)};

    int diffPosition {currentSize != newSize ?
                                             minSize
                                             : -1};

    if (!newHashes.size()) {
        diffPosition = 0;
    }

    for (int i = 0; i < minSize; ++i) {
        if (newHashes.at(i) != currentHashes.at(i)) {
            diffPosition = i;
            break;
        }
    }

    return diffPosition;
}

void HistoryWorker::requestCommandHashes() const
{
    BasicPackage package {QVariant::fromValue(true), // dummy data
                            networking::PType::COMMAND_HASHES_REQUEST};
    notifyClients(package);
}

void HistoryWorker::handleHistoryAction(const IPackage& package)
{
    switch (package.type()) {
        case networking::PType::HISTORY_HASH_UPDATE: {
            handleHistoryHashUpdate(package);
            break;
        }
        case networking::PType::COMMAND_HASHES_REQUEST: {
            handleHistoryHashesRequest(package);
            break;
        }
        case networking::PType::COMMAND_HASHES_RESPONSE: {
            handleHistoryHashesResponse(package);
            break;
        }
        case networking::PType::COMMANDS_REQUEST: {
            handleCommandsRequest(package);
            break;
        }
        case networking::PType::COMMANDS_RESPONSE: {
            handleCommandsResponse(package);
            break;
        }
        default: {
            qWarning() << "Invalid package type!";
        }
    }
}

void HistoryWorker::update()
{
    onHistoryChanged();
}

void HistoryWorker::startSynchronization(QTcpSocket* socket)
{
    if (socket) {
        sendHashUpdate(socket);
    }
}

void HistoryWorker::handleHistoryHashesRequest(const IPackage& request) const
{
    Q_UNUSED(request)
    sendCommandHashes();
}

void HistoryWorker::handleHistoryHashesResponse(const IPackage& response) const
{
    QVector<quint64> hashes {response.data().value<QVector<quint64>>()};

    int diffPosition {getHashesDiffPosition(hashes)};

    if (diffPosition != -1) {
        sendCommandRequest(static_cast<size_t>(diffPosition));
    }
}

void HistoryWorker::handleCommandsRequest(const IPackage& request) const
{
    const auto fromPosition = request.data().value<size_t>();

    QList<DrawCommandMemento> commands;
    commands.reserve(static_cast<int>(m_pHistory->size() - fromPosition));

    for (auto i = std::next(m_pHistory->begin(), static_cast<long>(fromPosition));
         i != m_pHistory->end();
         ++i) {
        if (const auto command = dynamic_cast<DrawCommand*>(i->get())) {
            commands.append(command->getMemento());
        }
    }

    sendCommands(commands, fromPosition);
}

void HistoryWorker::handleCommandsResponse(const IPackage& response) const
{
    const auto& responseData = response.data().value<QPair<QList<DrawCommandMemento>, quint64>>();

    QList<DrawCommandMemento> newCommands {responseData.first};
    size_t fromPosition {responseData.second};

    QSignalBlocker historyUpdateBlocker {m_pHistory};

    if (fromPosition < m_pHistory->size()) {
        const auto toDeleteCount {m_pHistory->size() - fromPosition};
        for (size_t i = 0; i < toDeleteCount; ++i) {
            m_pHistory->pop();
        }
    }

    for (const auto& commandMemento : newCommands) {
        auto command = DrawCommandFactory::createCommandByType(nullptr,
                                                               commandMemento.type());
        command->retrieveMemento(commandMemento);
        m_pHistory->add(std::move(command));
    }

    m_historyHash.calculate(*m_pHistory);
}

void HistoryWorker::sendCommandHashes() const
{
    BasicPackage package {QVariant::fromValue(m_historyHash.commandHashes()),
                            networking::PType::COMMAND_HASHES_RESPONSE};
    notifyClients(package);
}

void HistoryWorker::sendCommandRequest(size_t fromPosition) const
{
    BasicPackage package {QVariant::fromValue(fromPosition),
                            networking::PType::COMMANDS_REQUEST};

    notifyClients(package);
}

void HistoryWorker::sendCommands(const QList<DrawCommandMemento>& commands, quint64 fromPosition) const
{
    const auto commandsAndPosition {qMakePair(commands, fromPosition)};
    BasicPackage package {QVariant::fromValue(commandsAndPosition),
                            networking::PType::COMMANDS_RESPONSE};
    notifyClients(package);
}

void HistoryWorker::notifyClients(const IPackage& data) const
{
    for (const auto client : m_clients) {
        client->write(data.rawData());
    }
}

void HistoryWorker::handleHistoryHashUpdate(const IPackage& package) const
{
    const auto newHash = package.data().value<quint64>();
    if (!isValidHash(newHash)) {
        requestCommandHashes();
    }
}

void HistoryWorker::onHistoryChanged()
{
    m_historyHash.calculate(*m_pHistory);
    sendHashUpdate();
}

void HistoryWorker::sendHashUpdate() const
{
    BasicPackage package {QVariant::fromValue(m_historyHash.totalHash()),
                            networking::PType::HISTORY_HASH_UPDATE};
    notifyClients(package);
}

void HistoryWorker::sendHashUpdate(QTcpSocket* socket) const
{
    BasicPackage package {QVariant::fromValue(m_historyHash.totalHash()),
                            networking::PType::HISTORY_HASH_UPDATE};
    if (socket) {
        socket->write(package.rawData());
    }
}
