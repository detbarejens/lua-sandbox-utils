#pragma once
#include "../../FiveM-External.hpp"
#include "../../Utils/Misc.hpp"
#include <chrono>
#include <map>
#include <mutex>
#include "../../ImGui/imgui.h"
#include "../../ImGui/imgui_internal.h"

namespace NotifyManager
{
    enum eType {
        None,
        Info,
        Warning
    };

    enum eState {
        In,
        Current,
        Out,
        Expired
    };

    class NotifyClass {
    private:
        std::string Title;
        std::string Description;
        time_t ExpireTime = 0;
        time_t CreationTime = 0;
        eType Type = eType::None;
        eState CurrentState = eState::In;
    public:
        void SetTitle(std::string NewTitle) { this->Title = std::move(NewTitle); }
        void SetDescription(std::string NewDesc) { this->Description = std::move(NewDesc); }
        void SetType(eType NewType) { this->Type = NewType; }
        void SetState(eState NewState) { this->CurrentState = NewState; }
        void SetCreationTime(time_t NewCreationTime) { this->CreationTime = NewCreationTime; }
    public:
        const std::string& GetTitle() const { return Title; }
        const std::string& GetDescription() const { return Description; }
        time_t GetExpireTime() { return ExpireTime; }
        time_t GetCreationTime() { return CreationTime; }
        eType GetType() { return Type; }
        eState GetCurrentState() { return CurrentState; }
    public:
        time_t GetNowTime() {
            using namespace std::chrono;
            return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        }

        time_t GetTimeDiff() {
            return (time_t)(GetNowTime() - CreationTime);
        }

        NotifyClass(eType Type, time_t ExpireTime = 4000)
        {
            this->Type = Type;
            this->ExpireTime = ExpireTime;
            this->CreationTime = GetNowTime();
        }
    };

    inline std::vector<NotifyClass> NotifyList;
    inline std::mutex NotifyMutex;

    inline void DeleteNotify(int Index)
    {
        NotifyList.erase(NotifyList.begin() + Index);
    }

    inline ImU32 WithAlpha(int r, int g, int b, float alpha)
    {
        const float styleAlpha = ImGui::GetStyle().Alpha;
        return IM_COL32(r, g, b, (int)(alpha * styleAlpha * 255.f));
    }

    inline ImFont* ResolveNotifyFont()
    {
        ImFont* font = FrameWork::Assets::InterBold;
        if (font && font->IsLoaded())
            return font;

        ImGuiIO& io = ImGui::GetIO();
        if (io.Fonts && !io.Fonts->Fonts.empty())
            return io.Fonts->Fonts[0];

        return ImGui::GetFont();
    }

    inline void Render()
    {
        std::lock_guard<std::mutex> lock(NotifyMutex);

        const auto DrawList = ImGui::GetForegroundDrawList();
        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        const float WindowWidth = displaySize.x;
        const float WindowHeight = displaySize.y;
        const float EdgePadding = 20.f;
        float NextHeight = 0.f;

        ImFont* font = ResolveNotifyFont();
        const float titleFontSize = 18.f;
        const float descFontSize = 16.f;

        for (auto i = 0; i < NotifyList.size(); i++)
        {
            auto& Notify = NotifyList.at(i);

            struct NotifyAnim_t {
                float YPos = 0.f;
                float Alpha = 0.f;
            };

            static std::map<std::string, NotifyAnim_t> anim;
            const auto Id = Notify.GetDescription() + std::to_string(Notify.GetCreationTime());
            auto NotifyAnim = anim.find(Id);
            if (NotifyAnim == anim.end())
            {
                anim.insert({ Id, NotifyAnim_t() });
                NotifyAnim = anim.find(Id);
            }

            if (Notify.GetCurrentState() == eState::Expired)
            {
                Notify.SetState(eState::In);
                DeleteNotify(i);
                anim.erase(Id);
                i--;
                continue;
            }

            const std::string& titleText = Notify.GetTitle();
            const std::string& descText = Notify.GetDescription();

            auto TitleTxtSize = FrameWork::Misc::CalcTextSize(font, (int)titleFontSize, titleText.c_str());
            auto DescTxtSize = FrameWork::Misc::CalcTextSize(font, (int)descFontSize, descText.c_str());

            const float Padding = 16.f;
            const float MinWidth = 220.f;
            const float contentWidth = ImMax(TitleTxtSize.x, DescTxtSize.x);
            const ImVec2 NotifySize(
                ImMax(contentWidth + (Padding * 2.f), MinWidth),
                DescTxtSize.y + TitleTxtSize.y + (Padding * 2.f) + 8.f);

            if (Notify.GetCurrentState() == eState::In || Notify.GetCurrentState() == eState::Current)
            {
                NotifyAnim->second.Alpha = ImLerp(NotifyAnim->second.Alpha, 1.f, ImGui::GetIO().DeltaTime * 10.f);
                if (NotifyAnim->second.Alpha >= 0.98f)
                    Notify.SetState(eState::Current);
            }

            if (Notify.GetCurrentState() == eState::Current && Notify.GetTimeDiff() > Notify.GetExpireTime())
                Notify.SetState(eState::Out);

            if (Notify.GetCurrentState() == eState::Out)
            {
                NotifyAnim->second.Alpha = ImLerp(NotifyAnim->second.Alpha, 0.f, ImGui::GetIO().DeltaTime * 8.f);
                if (NotifyAnim->second.Alpha <= 0.02f)
                    Notify.SetState(eState::Expired);
            }

            NotifyAnim->second.YPos = ImLerp(NotifyAnim->second.YPos, NextHeight, ImGui::GetIO().DeltaTime * 8.f);

            const float NotifyStartX = WindowWidth - NotifySize.x - EdgePadding;
            const float NotifyStartY = WindowHeight - NotifySize.y - EdgePadding - NotifyAnim->second.YPos;
            const float fadeAlpha = NotifyAnim->second.Alpha;

            const ImVec2 boxMin(NotifyStartX, NotifyStartY);
            const ImVec2 boxMax(NotifyStartX + NotifySize.x, NotifyStartY + NotifySize.y);
            const ImU32 bgColor = WithAlpha(14, 14, 14, fadeAlpha);
            const ImU32 titleColor = WithAlpha(200, 200, 200, fadeAlpha);
            const ImU32 descColor = WithAlpha(130, 130, 130, fadeAlpha);
            const ImU32 barColor = WithAlpha(120, 120, 120, fadeAlpha);

            DrawList->AddRectFilled(boxMin, boxMax, bgColor, 6.f);

            const float Progress = 1.0f - ((float)Notify.GetTimeDiff() / (float)Notify.GetExpireTime());
            const ImVec2 ProgressBarMin(boxMin.x, boxMax.y - 3.f);
            const ImVec2 ProgressBarMax(boxMin.x + (NotifySize.x * Progress), boxMax.y - 1.f);
            DrawList->AddRectFilled(ProgressBarMin, ProgressBarMax, barColor, 1.f);

            if (font && !titleText.empty())
            {
                DrawList->PushTextureID(font->ContainerAtlas->TexID);
                const ImVec2 TitlePos(boxMin.x + Padding, boxMin.y + Padding - 1.f);
                DrawList->AddText(font, titleFontSize, TitlePos, titleColor, titleText.c_str());

                if (!descText.empty())
                {
                    const ImVec2 DescPos(TitlePos.x, TitlePos.y + TitleTxtSize.y + 4.f);
                    DrawList->AddText(font, descFontSize, DescPos, descColor, descText.c_str());
                }

                DrawList->PopTextureID();
            }

            NextHeight += NotifySize.y + 10.f;
        }
    }

    inline void Send(std::string Description, time_t ExpireTime = 4000)
    {
        NotifyClass Notify(eType::Info, ExpireTime);
        Notify.SetTitle(std::string(XorStr("pksd bypass")));
        Notify.SetDescription(std::move(Description));

        std::lock_guard<std::mutex> lock(NotifyMutex);
        NotifyList.push_back(std::move(Notify));
    }

}
