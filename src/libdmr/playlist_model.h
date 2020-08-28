/*
 * (c) 2017, Deepin Technology Co., Ltd. <support@deepin.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * is provided AS IS, WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, and
 * NON-INFRINGEMENT.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
#ifndef _DMR_PLAYLIST_MODEL_H
#define _DMR_PLAYLIST_MODEL_H

#include <QtWidgets>
#include <QtConcurrent>
#include <libffmpegthumbnailer/videothumbnailer.h>

#include "utils.h"
#include <QNetworkReply>
#include <QMutex>
namespace dmr {
using namespace ffmpegthumbnailer;
class PlayerEngine;
class LoadThread;

struct MovieInfo {
    bool valid;
    QString title;
    QString fileType;
    QString resolution;
    QString filePath;
    QString creation;

    // rotation in metadata, this affects width/height
    int raw_rotate;
    qint64 fileSize;
    qint64 duration;
    int width, height;

    //3.4添加视频信息
    //视频流信息
    int vCodecID;
    qint64 vCodeRate;
    int fps;
    float proportion;
    //音频流信息
    int aCodeID;
    qint64 aCodeRate;
    int aDigit;
    int channels;
    int sampling;

    static struct MovieInfo parseFromFile(const QFileInfo &fi, bool *ok = nullptr);
    QString durationStr() const
    {
        return utils::Time2str(duration);
    }

    QString videoCodec() const
    {
        return  utils::videoIndex2str(vCodecID);
    }

    QString audioCodec() const
    {
        return utils::audioIndex2str(aCodeID);
    }

    //获取字幕编码格式（备用）
    /*QString subtitleCodec() const
    {
        return utils::subtitleIndex2str();
    }*/

    QString sizeStr() const
    {
        auto K = 1024;
        auto M = 1024 * K;
        auto G = 1024 * M;
        if (fileSize > G) {
            return QString(QT_TR_NOOP("%1G")).arg((double)fileSize / G, 0, 'f', 1);
        } else if (fileSize > M) {
            return QString(QT_TR_NOOP("%1M")).arg((double)fileSize / M, 0, 'f', 1);
        } else if (fileSize > K) {
            return QString(QT_TR_NOOP("%1K")).arg((double)fileSize / K, 0, 'f', 1);
        }
        return QString(QT_TR_NOOP("%1")).arg(fileSize);
    }
};


struct PlayItemInfo {
    bool valid;
    bool loaded;  // if url is network, this is false until playback started
    QUrl url;
    QFileInfo info;
    QPixmap thumbnail;
    struct MovieInfo mi;

    bool refresh();
};

using AppendJob = QPair<QUrl, QFileInfo>; // async job
using PlayItemInfoList = QList<PlayItemInfo>;
using UrlList = QList<QUrl>;


class PlaylistModel: public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int current READ current WRITE changeCurrent NOTIFY currentChanged)

public:
    friend class PlayerEngine;
    enum PlayMode {
        OrderPlay,
        ShufflePlay,
        SinglePlay,
        SingleLoop,
        ListLoop,
    };

    void stop();

    PlayMode playMode() const;
    void setPlayMode(PlayMode pm);

    PlaylistModel(PlayerEngine *engine);
    ~PlaylistModel();

    qint64 getUrlFileTotalSize(QUrl url, int tryTimes) const;

    void clear();
    void remove(int pos);
    void append(const QUrl &);

    void appendAsync(const QList<QUrl> &);
    void collectionJob(const QList<QUrl> &);

    void playNext(bool fromUser);
    void playPrev(bool fromUser);

    int count() const;
    const QList<PlayItemInfo> &items() const
    {
        return _infos;
    }
    QList<PlayItemInfo> &items()
    {
        return _infos;
    }

    int current() const;
    const PlayItemInfo &currentInfo() const;
    PlayItemInfo &currentInfo();
    int indexOf(const QUrl &url);

    void switchPosition(int p1, int p2);

    bool hasPendingAppends();
    void handleAsyncAppendResults(QList<PlayItemInfo> &pil);
    struct PlayItemInfo calculatePlayInfo(const QUrl &, const QFileInfo &fi, bool isDvd = false);
    bool getthreadstate();

public slots:
    void changeCurrent(int);
    void delayedAppendAsync(const QList<QUrl> &);
    void deleteThread();

private slots:
    void onAsyncAppendFinished();


signals:
    void countChanged();
    void currentChanged();
    void itemRemoved(int);
    void itemsAppended();
    void emptied();
    void playModeChanged(PlayMode);
    void asyncAppendFinished(const QList<PlayItemInfo> &);
    void itemInfoUpdated(int id);

private:
    // when app starts, and the first time to load playlist
    bool _firstLoad {true};
    int _count {0};
    int _current {-1};
    int _last {-1};
    PlayMode _playMode {PlayMode::OrderPlay};
    QList<PlayItemInfo> _infos;

    QList<int> _playOrder; // for shuffle mode
    int _shufflePlayed {0}; // count currently played items in shuffle mode
    int _loopCount {0}; // loop count

    QList<AppendJob> _pendingJob; // async job
    QSet<QString> _urlsInJob;  // url list
    QFutureWatcher<PlayItemInfo> *_jobWatcher {nullptr};

    QQueue<UrlList> _pendingAppendReq;

    bool _userRequestingItem {false};

    VideoThumbnailer _thumbnailer;
    PlayerEngine *_engine {nullptr};

    QString _playlistFile;

    LoadThread *m_ploadThread;
    QMutex *m_pdataMutex;
    bool m_brunning;

    void reshuffle();
    void savePlaylist();
    void loadPlaylist();
    void clearPlaylist();
    void appendSingle(const QUrl &);
    void tryPlayCurrent(bool next);

};


class LoadThread: public QThread
{
    Q_OBJECT

public:
    LoadThread(PlaylistModel *model, const QList<QUrl> &urls);
    ~LoadThread();

public:
    void run();

private:
    PlaylistModel *_pModel;
    QList<QUrl> _urls;

    QList<AppendJob> _pendingJob; // async job
    QSet<QString> _urlsInJob;  // url list
};


}

#endif /* ifndef _DMR_PLAYLIST_MODEL_H */

